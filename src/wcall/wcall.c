/*
 * Wire
 * Copyright (C) 2016 Wire Swiss GmbH
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <pthread.h>

#include <re/re.h>
#include <avs.h>

#include <avs_wcall.h>
#include <avs_vie.h>

#include "wcall.h"


#define AUDIO_CBR_STATE_UNSET (-1)

#define APITAG "WAPI "


static struct {
	struct list logl;
} calling;


struct log_entry {
	struct log logger;

	wcall_log_h *logh;
	void *arg;

	struct le le;
};


struct calling_instance {
	struct wcall_marshal *marshal;
	struct mediamgr *mm;
	char *userid;
	char *clientid;
	struct ecall_conf conf;
	struct call_config *call_config;
	struct lock *lock;
	struct msystem *msys;
	struct config *cfg;

	struct list ecalls;
	struct list wcalls;
	struct list ctxl;

	pthread_t tid;
	bool thread_run;

	wcall_ready_h *readyh;
	wcall_send_h *sendh;
	wcall_incoming_h *incomingh;
	wcall_missed_h *missedh;
	wcall_answered_h *answerh;
	wcall_estab_h *estabh;
	wcall_close_h *closeh;
	wcall_metrics_h *metricsh;
	wcall_config_req_h *cfg_reqh;
	wcall_state_change_h *stateh;
	wcall_video_state_change_h *vstateh;
	wcall_audio_cbr_change_h *acbrh;
	wcall_data_chan_estab_h *dcestabh;
	struct {
		wcall_group_changed_h *chgh;
		void *arg;
	} group;
	
	void *arg;

	struct tmr tmr_roam;

	struct le le;
};

static struct list instances = LIST_INIT;

struct wcall {
	struct calling_instance *inst;
	char *convid;

	struct ecall *ecall;

	struct {
		bool video_call;
		bool send_active;
		int recv_state;
	} video;

	struct {
		int cbr_state;
	} audio;
	struct {
		struct egcall *call;
	} group;
    
	int state; /* wcall state */
	
	struct le le;
};


struct wcall_ctx {
	struct calling_instance *inst;
	struct wcall *wcall;
	void *context;

	struct le le; /* member of ctxl */
};


static bool wcall_has_calls(struct calling_instance *inst);


static struct sa *url2sa(struct sa *sa, const char *url)
{
	struct uri uri;
	struct pl pl_uri;
	int err = 0;
		
	pl_set_str(&pl_uri, url);
	err = uri_decode(&uri, &pl_uri);
	if (err) {
		warning("cannot decode URI (%r)\n", &pl_uri);
		goto out;
	}

	if (0 == pl_strcasecmp(&uri.scheme, "turn")) {
		err = sa_set(sa, &uri.host, uri.port);
		if (err)
			goto out;

	}
	else {
		warning("msystem: get_turn_servers: unknown URI scheme"
			" '%r'\n", &uri.scheme);
		err = ENOTSUP;
		goto out;
	}

 out:
	if (err)
		return NULL;

	return sa;
}


const char *wcall_state_name(int st)
{	
	switch(st) {
	case WCALL_STATE_NONE:
		return "none";
		
	case WCALL_STATE_OUTGOING:
		return "outgoing";
		
	case WCALL_STATE_INCOMING:
		return "incoming";
		
	case WCALL_STATE_ANSWERED:
		return "answered";

	case WCALL_STATE_MEDIA_ESTAB:
		return "media-established";
		
	case WCALL_STATE_TERM_LOCAL:
		return "locally terminated";

	case WCALL_STATE_TERM_REMOTE:
		return "remotely terminated";
		
	case WCALL_STATE_UNKNOWN:
		return "unknown";

	default:
		return "?";
	}
}


static void set_state(struct wcall *wcall, int st)
{
	bool trigger = wcall->state != st;
	struct calling_instance *inst = wcall->inst;
	
	info("wcall(%p): set_state: %s->%s\n",
	     wcall, wcall_state_name(wcall->state), wcall_state_name(st));
	
	wcall->state = st;

	if (trigger && inst && inst->stateh) {
		inst->stateh(wcall->convid, wcall->state, inst->arg);
	}
}


static void *cfg_wait_thread(void *arg)
{
	struct calling_instance *inst = arg;

	(void)arg;

	debug("wcall(%p): starting config wait\n", inst);	
	while (inst->call_config == NULL && inst->thread_run) {
		sys_msleep(500);
	}
	if (inst->call_config)
		debug("wcall(%p): config ready!\n", inst);

	return NULL;
}

static int async_cfg_wait(struct calling_instance *inst)
{
	int err;

	info("wcall: creating async config polling thread\n");
	
	inst->thread_run = true;
	err = pthread_create(&inst->tid, NULL, cfg_wait_thread, inst);
	if (err) {
		inst->thread_run = false;
		return err;
	}

	return err;
}


struct wcall *wcall_lookup(void *id, const char *convid)
{
	struct calling_instance *inst = id;
	struct wcall *wcall;
	bool found = false;
	struct le *le;

	if (!id || !convid)
		return NULL;
	
	lock_write_get(inst->lock);
	for (le = inst->wcalls.head;
	     le != NULL && !found;
	     le = le->next) { 
		wcall = le->data;

		if (streq(convid, wcall->convid)) {
			found = true;
		}
	}
	lock_rel(inst->lock);
	
	return found ? wcall : NULL;
}


static void invoke_incoming_handler(const char *convid,
				    uint32_t msg_time,
				    const char *userid,
				    int video_call,
				    int should_ring,
				    void *arg)
{
	struct calling_instance *inst = arg;
	uint64_t now = tmr_jiffies();


	info(APITAG "wcall(%p): calling incomingh: %p\n",
	     inst, inst->incomingh);

	if (inst->incomingh) {
		inst->incomingh(convid, msg_time, userid,
				video_call, should_ring, inst->arg);
	}
		
	info(APITAG "wcall(%p): inst->incomingh took %ld ms \n",
	     inst, tmr_jiffies() - now);
}



static void ecall_propsync_handler(void *arg);

static void egcall_start_handler(uint32_t msg_time,
				 const char *userid_sender,
				 bool should_ring,
				 void *arg)
{
	struct wcall *wcall = arg;
	struct calling_instance *inst = wcall->inst;

	set_state(wcall, WCALL_STATE_INCOMING);

	info(APITAG "wcall(%p): incomingh(%p) group:yes video:no ring:%s\n",
		wcall, wcall->inst->incomingh, should_ring ? "yes" : "no");

#ifndef ANDROID // Breaks ringtone for Android. Speeds up call setup for ios ...
	if (inst->mm) {
		mediamgr_set_call_state(inst->mm,
					MEDIAMGR_STATE_INCOMING_AUDIO_CALL);
	}
#endif
	
	if (inst->incomingh) {
		if (inst->mm) {
			mediamgr_invoke_incomingh(inst->mm,
						  invoke_incoming_handler,
						  wcall->convid, msg_time,
						  userid_sender,
						  0,
						  should_ring ? 1 : 0,
						  inst);
		}
		else {
			invoke_incoming_handler(wcall->convid, msg_time,
						userid_sender, 0,
						should_ring ? 1 : 0, inst);
		}
	}
}


static void ecall_conn_handler(struct ecall *ecall,
			       uint32_t msg_time,
			       const char *userid_sender,
			       bool video_call,
			       void *arg)
{
	struct wcall *wcall = arg;
	struct calling_instance *inst = wcall->inst;

	// TODO: lookup userid

	wcall->video.video_call = video_call && ecall_has_video(wcall->ecall);

	info(APITAG "wcall(%p): incomingh(%p) group:no video:%s ring:yes\n",
		 wcall, inst->incomingh, video_call ? "yes" : "no");

	set_state(wcall, WCALL_STATE_INCOMING);

#ifndef ANDROID // Breaks ringtone for Android. Speeds up call setup for ios ...
	if (inst->mm) {
		enum mediamgr_state state;

		state = wcall->video.video_call ?
			MEDIAMGR_STATE_INCOMING_VIDEO_CALL :
			MEDIAMGR_STATE_INCOMING_AUDIO_CALL;

		mediamgr_set_call_state(inst->mm, state);
	}
#endif
    
	if (inst->incomingh) {
		if (inst->mm) {
			mediamgr_invoke_incomingh(inst->mm,
						  invoke_incoming_handler,
						  wcall->convid, msg_time,
						  userid_sender,
						  video_call ? 1 : 0, 1, inst);
		}
		else {
			invoke_incoming_handler(wcall->convid, msg_time,
						userid_sender,
						video_call ? 1 : 0,
						1,
						inst);
		}
	}

	ecall_propsync_handler(arg);
}


static void ecall_answer_handler(void *arg)
{
	struct wcall *wcall = arg;
	struct calling_instance *inst = wcall->inst;

	info(APITAG "wcall(%p): answerh(%p) convid=%s\n", wcall, inst->answerh, wcall->convid);
	if (inst->answerh) {
		uint64_t now = tmr_jiffies();
		inst->answerh(wcall->convid, inst->arg);

		info(APITAG "wcall(%p): answerh took %ld ms \n",
		     wcall, tmr_jiffies() - now);
	}
	set_state(wcall, WCALL_STATE_ANSWERED);
	ecall_propsync_handler(arg);
}


static void ecall_media_estab_handler(struct ecall *ecall, bool update,
				      void *arg)
{
	int err;
	struct wcall *wcall = arg;
	struct calling_instance *inst = wcall->inst;
	const char *peer_userid = ecall_get_peer_userid(ecall);

	(void)ecall;

	info("wcall(%p): media established(video=%d): "
	     "convid=%s peer_userid=%s update=%d\n",
	     wcall, wcall->video.video_call,
	     wcall->convid, peer_userid, update);
	
	set_state(wcall, WCALL_STATE_MEDIA_ESTAB);
    
	if (inst->mm) {
		enum mediamgr_state state;

		state = wcall->video.video_call ? MEDIAMGR_STATE_INVIDEOCALL
			                        : MEDIAMGR_STATE_INCALL;
		mediamgr_set_call_state(inst->mm, state);
	}
	else {
		err = ecall_media_start(ecall);
		if (err) {
			warning("wcall(%p): ecall_media_start failed (%m)\n",
				wcall, err);
		}
	}
}


static void ecall_audio_estab_handler(struct ecall *ecall, bool update,
				      void *arg)
{
	struct wcall *wcall = arg;
	struct calling_instance *inst = wcall->inst;
	const char *peer_userid = ecall_get_peer_userid(ecall);

	(void)ecall;
	
	info("wcall(%p): audio established(video=%d): "
	     "convid=%s peer_userid=%s inst=%p mm=%p\n",
	     wcall, wcall->video.video_call, wcall->convid, peer_userid,
	     inst, inst->mm);
    
	// TODO: check this, it should not be necessary
	msystem_stop_silencing();
	
    
	info(APITAG "wcall(%p): estabh(%p) peer_userid=%s\n",
	     wcall, inst->estabh, peer_userid);

	if (!update && inst->estabh) {
		uint64_t now = tmr_jiffies();
		inst->estabh(wcall->convid, peer_userid, inst->arg);

		info(APITAG "wcall(%p): estabh took %ld ms \n",
		     wcall, tmr_jiffies() - now);
	}
}

static void ecall_datachan_estab_handler(struct ecall *ecall, bool update,
					 void *arg)
{
	struct wcall *wcall = arg;
	const char *peer_userid = ecall_get_peer_userid(ecall);

	info("wcall(%p): data channel established for conversation %s "
	     "update=%d\n",
	     wcall, wcall->convid, update);

	info(APITAG "wcall(%p): dcestabh(%p) "
	     "conv=%s peer_userid=%s update=%d\n",
	     wcall, wcall->inst ? wcall->inst->dcestabh : NULL,
	     wcall->convid, peer_userid, update);

	if (wcall->inst && wcall->inst->dcestabh) {
		uint64_t now = tmr_jiffies();
		wcall->inst->dcestabh(wcall->convid,
				      peer_userid,
				      wcall->inst->arg);

		info(APITAG "wcall(%p): dcestabh took %ld ms \n",
		     wcall, tmr_jiffies() - now);
	}
}


static void ecall_propsync_handler(void *arg)
{
	struct wcall *wcall = arg;
	struct calling_instance *inst = wcall->inst;
	int state = WCALL_VIDEO_RECEIVE_STOPPED;
	bool vstate_changed = false;
	const char *vr, *cr, *cl;
	bool local_cbr, remote_cbr, cbr_enabled;

	if (!wcall) {
		warning("wcall(%p): propsyc_handler wcall is NULL, "
			"ignoring props\n", wcall);
		return;
	}

	if (!wcall->ecall) {
		warning("wcall(%p): propsyc_handler ecall is NULL, "
			"ignoring props\n", wcall);
		return;
	}

	info("wcall(%p): propsync_handler, current recv_state %s \n",
	     wcall, wcall->video.recv_state ? "true" : "false");

	vr = ecall_props_get_remote(wcall->ecall, "videosend");
	if (vr) {
		vstate_changed = true;
		if (strcmp(vr, "true") == 0) {
			state = WCALL_VIDEO_RECEIVE_STARTED;
		}
	}

	vr = ecall_props_get_remote(wcall->ecall, "screensend");
	if (vr) {
		vstate_changed = true;
		if (strcmp(vr, "true") == 0) {
			state = WCALL_VIDEO_RECEIVE_STARTED;
		}
	}
    
	if (vstate_changed && state != wcall->video.recv_state) {
		info("wcall(%p): propsync_handler updating recv_state "
		     "%s -> %s\n",
		     wcall,
		     wcall->video.recv_state ? "true" : "false",
		     state ? "true" : "false");
		wcall->video.recv_state = state;
		info(APITAG "wcall(%p): vstateh(%p) state=%d\n",
		     wcall, inst->vstateh, state);
		if (inst->vstateh) {
			uint64_t now = tmr_jiffies();
			inst->vstateh(state, inst->arg);
			info(APITAG "wcall(%p): vstateh took %ld ms\n",
			     wcall, tmr_jiffies() - now);
		}
	}
    
	cl = ecall_props_get_local(wcall->ecall, "audiocbr");
	local_cbr = cl && streq(cl, "true");

	cr = ecall_props_get_remote(wcall->ecall, "audiocbr");
	remote_cbr = cr && streq(cr, "true");
		
	cbr_enabled = local_cbr && remote_cbr ? 1 : 0;

	if (cbr_enabled != wcall->audio.cbr_state) {
		info("wcall(%p): acbrh(%p) lcbr=%s rcbr=%s cbr=%d\n", wcall, inst->acbrh,
			local_cbr ? "true" : "false", remote_cbr ? "true" : "false",
			cbr_enabled);
		if (inst->acbrh) {
			uint64_t now = tmr_jiffies();
			inst->acbrh(cbr_enabled, inst->arg);
			info("wcall(%p): acbrh took %ld ms\n",
			     wcall, tmr_jiffies() - now);
			wcall->audio.cbr_state = cbr_enabled;
		}
	}
}


static int err2reason(int err)
{
	switch (err) {

	case 0:
		return WCALL_REASON_NORMAL;

	case ETIMEDOUT:
		return WCALL_REASON_TIMEOUT;

	case ETIMEDOUT_ECONN:
		return WCALL_REASON_TIMEOUT_ECONN;

	case ECONNRESET:
		return WCALL_REASON_LOST_MEDIA;

	case ECANCELED:
		return WCALL_REASON_CANCELED;

	case EALREADY:
		return WCALL_REASON_ANSWERED_ELSEWHERE;

	case EIO:
		return WCALL_REASON_IO_ERROR;
            
	case EDATACHANNEL:
		return WCALL_REASON_DATACHANNEL;

	case EREMOTE:
		return WCALL_REASON_REJECTED;

	default:
		/* TODO: please convert new errors */
		warning("wcall: default reason (%d) (%m)\n", err, err);
		return WCALL_REASON_ERROR;
	}
}


static void ecall_close_handler(int err, const char *metrics_json,
				struct ecall *ecall, uint32_t msg_time,
				void *arg)
{
	struct wcall *wcall = arg;
	struct calling_instance *inst = wcall->inst;
	const char *userid;
	int reason;
	
	reason = err2reason(err);

	info(APITAG "wcall(%p): closeh(%p) group=no state=%s reason=%s\n",
	     wcall, inst->closeh, wcall_state_name(wcall->state),
	     wcall_reason_name(reason));

	/* If the call was already rejected, we don't want to
	 * do anything here
	 */
	if (wcall->state == WCALL_STATE_NONE)
		goto out;

	if (wcall->state == WCALL_STATE_TERM_LOCAL)
		userid = inst->userid;
	else {
		wcall->state = WCALL_STATE_TERM_REMOTE;
		userid = ecall ? ecall_get_peer_userid(ecall) : inst->userid;
	}

	set_state(wcall, WCALL_STATE_NONE);
	if (inst->closeh) {
		uint64_t now = tmr_jiffies();
		inst->closeh(reason, wcall->convid, msg_time, userid, inst->arg);
		info(APITAG "wcall(%p): closeh took %ld ms\n",
		     wcall, tmr_jiffies() - now);
	}

	info(APITAG "wcall(%p): metricsh(%p) json=%p\n", wcall, inst->metricsh, metrics_json);

	if (inst->metricsh && metrics_json) {
		uint64_t now = tmr_jiffies();
		inst->metricsh(wcall->convid, metrics_json, inst->arg);
		info(APITAG "wcall(%p): metricsh took %ld ms\n",
		     wcall, tmr_jiffies() - now);
	}
out:
	mem_deref(wcall);
}


static void egcall_leave_handler(int reason, uint32_t msg_time, void *arg)
{
	struct wcall *wcall = arg;
	struct calling_instance *inst = wcall->inst;

	info(APITAG "wcall(%p): closeh(%p) group=yes state=%s reason=%s\n",
	     wcall, inst->closeh, wcall_state_name(wcall->state),
	     wcall_reason_name(reason));

	set_state(wcall, WCALL_STATE_INCOMING);
	if (inst->closeh) {
		int wreason = WCALL_REASON_NORMAL;

		switch (reason) {
		case EGCALL_REASON_STILL_ONGOING:
			wreason = WCALL_REASON_STILL_ONGOING;
			break;
		case EGCALL_REASON_ANSWERED_ELSEWHERE:
			wreason = WCALL_REASON_ANSWERED_ELSEWHERE;
			break;
		case EGCALL_REASON_REJECTED:
			wreason = WCALL_REASON_REJECTED;
			break;
		default:
			wreason = WCALL_REASON_NORMAL;
			break;
		}
		uint64_t now = tmr_jiffies();
		inst->closeh(wreason, wcall->convid, msg_time, inst->userid, inst->arg);
		info(APITAG "wcall(%p): closeh took %ld ms\n",
		     wcall, tmr_jiffies() - now);
	}
}


static void egcall_group_changed_handler(void *arg)
{
	struct wcall *wcall = arg;
	struct calling_instance *inst = wcall->inst;

	if (inst->group.chgh)
		inst->group.chgh(wcall->convid, inst->group.arg);
}

static void egcall_metrics_handler(const char *metrics_json, void *arg)
{
	struct wcall *wcall = arg;
	struct calling_instance *inst = wcall->inst;

	if (inst->metricsh && metrics_json) {
		inst->metricsh(wcall->convid, metrics_json, inst->arg);
	}
}

static void egcall_close_handler(int err, uint32_t msg_time, void *arg)
{
	ecall_close_handler(err, NULL, NULL, msg_time, arg);
}

static void ctx_destructor(void *arg)
{
	struct wcall_ctx *ctx = arg;
	struct calling_instance *inst = ctx->inst;

	lock_write_get(inst->lock);
	list_unlink(&ctx->le);
	lock_rel(inst->lock);
}

static int ctx_alloc(struct wcall_ctx **ctxp, struct calling_instance *inst, void *context)
{
	struct wcall_ctx *ctx;

	if (!ctxp)
		return EINVAL;
	
	ctx = mem_zalloc(sizeof(*ctx), ctx_destructor);
	if (!ctx)
		return ENOMEM;

	ctx->context = context;
	ctx->inst = inst;
	lock_write_get(inst->lock);
	list_append(&inst->ctxl, &ctx->le, ctx);
	lock_rel(inst->lock);

	*ctxp = ctx;
	
	return 0;
}

static int ecall_send_handler(const char *userid,
			      struct econn_message *msg, void *arg)
{	
	struct wcall *wcall = arg;
	struct calling_instance *inst = wcall->inst;
	struct wcall_ctx *ctx;
	void *context = wcall;
	char *str = NULL;
	int err = 0;
	
	if (inst->sendh == NULL)
		return ENOSYS;
       
	err = ctx_alloc(&ctx, wcall->inst, context);
	if (err)
		return err;	

	err = econn_message_encode(&str, msg);
	if (err)
		return err;

	
	info("wcall(%p): c3_message_send: convid=%s from=%s.%s to=%s.%s "
	     "msg=%H ctx=%p\n",
	     wcall, wcall->convid, userid, inst->clientid,
	     strlen(msg->dest_userid) > 0 ? msg->dest_userid : "ALL",
	     strlen(msg->dest_clientid) > 0 ? msg->dest_clientid : "ALL",
	     econn_message_brief, msg, ctx);

	char *dest_user = NULL;
	char *dest_client = NULL;    
	if (strlen(msg->dest_userid) > 0) {
		dest_user = msg->dest_userid;
	}

	if (strlen(msg->dest_clientid) > 0) {
		dest_client = msg->dest_clientid;
	}
	err = inst->sendh(ctx, wcall->convid, userid, inst->clientid,
			    dest_user, dest_client, (uint8_t *)str, strlen(str),
			    msg->transient ? 1 : 0, inst->arg);
    
	mem_deref(str);

	return err;
}


static void destructor(void *arg)
{
	struct wcall *wcall = arg;
	struct calling_instance *inst = wcall->inst;
	bool has_calls;

	info("wcall(%p): dtor -- started\n", wcall);
	
	lock_write_get(inst->lock);
	list_unlink(&wcall->le);
	has_calls = inst->wcalls.head != NULL;
	lock_rel(inst->lock);

	if (!has_calls) {
		if (inst->mm) {
			mediamgr_set_call_state(inst->mm,
						MEDIAMGR_STATE_NORMAL);
		}
	}

	mem_deref(wcall->ecall);
	mem_deref(wcall->group.call);
	mem_deref(wcall->convid);

	info("wcall(%p): dtor -- done\n", wcall);
}


int wcall_add(void *id,
	      struct wcall **wcallp,
	      const char *convid,
	      bool group)
{
	struct calling_instance *inst = id;
	struct wcall *wcall;
	struct zapi_ice_server *turnv = NULL;
	size_t turnc = 0;
	size_t i;
	int err;

	if (!id || !wcallp || !convid)
		return EINVAL;

	wcall = wcall_lookup(id, convid);
	if (wcall) {
		warning("wcall(%p): call_add: already have wcall "
			"for convid=%s\n", wcall, convid);

		return EALREADY;
	}

	wcall = mem_zalloc(sizeof(*wcall), destructor);
	if (!wcall)
		return EINVAL;

	wcall->inst = inst;

	info(APITAG "wcall(%p): added for convid=%s inst=%p\n", wcall, convid, inst);
	str_dup(&wcall->convid, convid);

	lock_write_get(inst->lock);

	turnv = config_get_iceservers(inst->cfg, &turnc);
	if (turnc == 0) {
		info("wcall(%p): no turn servers\n", wcall);
	}

	if (group) {
		err = egcall_alloc(&wcall->group.call,
				   &inst->conf,
				   convid,
				   inst->userid,
				   inst->clientid,
				   ecall_send_handler,
				   egcall_start_handler,
				   ecall_media_estab_handler,
				   ecall_audio_estab_handler,
				   ecall_datachan_estab_handler,
				   egcall_group_changed_handler,
				   egcall_leave_handler,
				   egcall_close_handler,
				   egcall_metrics_handler,
				   wcall);
		if (err) {
			warning("wcall(%p): add: could not alloc egcall: %m\n",
				wcall, err);
			goto out;
		}
	}
	else {
		err = ecall_alloc(&wcall->ecall, &inst->ecalls,
				  &inst->conf, flowmgr_msystem(),
				  convid,
				  inst->userid,
				  inst->clientid,
				  ecall_conn_handler,
				  ecall_answer_handler,
				  ecall_media_estab_handler,
				  ecall_audio_estab_handler,
				  ecall_datachan_estab_handler,
				  ecall_propsync_handler,
				  ecall_close_handler,
				  ecall_send_handler,
				  wcall);
		if (err) {
			warning("wcall(%p): call_add: ecall_alloc "
				"failed: %m\n", wcall, err);
			goto out;
		}
	}
	for (i = 0; i < turnc; ++i) {
		struct zapi_ice_server *turn = &turnv[i];
		struct sa sa;

		if (group) {
			err = egcall_set_turnserver(wcall->group.call,
						    url2sa(&sa, turn->url),
						    turn->username,
						    turn->credential);
			if (err)
				goto out;
		}
		else {
			err = ecall_add_turnserver(wcall->ecall,
						   url2sa(&sa, turn->url),
						   turn->username,
						   turn->credential);
			info("wcall: setting ecall turn: %J %s:%s\n",
			     &sa, turn->username, turn->credential);
			
			if (err)
				goto out;
		}
	}

	wcall->video.recv_state = WCALL_VIDEO_RECEIVE_STOPPED;
	wcall->audio.cbr_state = AUDIO_CBR_STATE_UNSET;

	list_append(&inst->wcalls, &wcall->le, wcall);
	
 out:
	lock_rel(inst->lock);

	if (err)
		mem_deref(wcall);
	else
		*wcallp = wcall;

	return err;
}

static void mm_mcat_changed(enum mediamgr_state state, void *arg)
{
	struct calling_instance *inst = arg;

	wcall_mcat_changed(inst, state);
}


void wcall_i_mcat_changed(void *id, enum mediamgr_state state)
{
	struct le *le;
	struct calling_instance *inst = id;
	
	info("wcall: mcat changed to: %d inst=%p\n", state, inst);

	if (!inst) {
		warning("wcall_i mcat_changed: no instance\n");
		return;
	}

	lock_write_get(inst->lock);
	LIST_FOREACH(&inst->wcalls, le) {
		struct wcall *wcall = le->data;
	
		switch(state) {
		case MEDIAMGR_STATE_INCALL:
		case MEDIAMGR_STATE_INVIDEOCALL:
		case MEDIAMGR_STATE_RESUME:
			if (wcall->group.call) {
				egcall_media_start(wcall->group.call);
			}
			else {
				ecall_media_start(wcall->ecall);
			}
			break;
		
		case MEDIAMGR_STATE_HOLD:
			if (wcall->group.call) {
				egcall_media_stop(wcall->group.call);
			}
			else {
				ecall_media_stop(wcall->ecall);
			}
			break;

		case MEDIAMGR_STATE_NORMAL:
		default:
			//ecall_media_stop(wcall->ecall);
			break;
		}
	}
	lock_rel(inst->lock);	
}

static void mm_audio_route_changed(enum mediamgr_auplay new_route, void *arg)
{
	struct calling_instance *inst = arg;

	wcall_audio_route_changed(inst, new_route);
}

void wcall_i_audio_route_changed(enum mediamgr_auplay new_route)
{
	const char *dev;
	switch (new_route) {

		case MEDIAMGR_AUPLAY_EARPIECE:
			dev = "earpiece";
			break;

		case MEDIAMGR_AUPLAY_SPEAKER:
			dev = "speaker";
			break;

		case MEDIAMGR_AUPLAY_BT:
			dev = "bt";
			break;

		case MEDIAMGR_AUPLAY_LINEOUT:
			dev = "lineout";
			break;

		case MEDIAMGR_AUPLAY_SPDIF:
			dev = "spdif";
			break;

		case MEDIAMGR_AUPLAY_HEADSET:
			dev = "headset";
			break;

		default:
			warning("wcall: Unknown Audio route %d \n", new_route);
			return;
	}
	msystem_set_auplay(dev);
}

AVS_EXPORT
int wcall_init(void)
{
	int err = 0;
	debug("wcall: init\n");

	list_init(&calling.logl);

	return err;
}

AVS_EXPORT
void wcall_close(void)
{
	struct le *le;

	LIST_FOREACH(&calling.logl, le) {
		struct log_entry *loge = le->data;
		
		log_unregister_handler(&loge->logger);
	}
	list_flush(&calling.logl);
}


static int config_req_handler(void *arg)
{
	struct calling_instance *inst = arg;
	int err = 0;

	if (!inst)
		return EINVAL;

	if (inst->cfg_reqh) {
		err = inst->cfg_reqh(inst, inst->arg);
	}

	return err;
}


static void config_update_handler(struct call_config *cfg, void *arg)
{
	struct calling_instance *inst = arg;
	bool first = false;	
	
	if (cfg == NULL)
		return;

	if (inst->call_config == NULL)
		first = true;
	inst->call_config = cfg;

	debug("wcall(%p): call_config: %d ice servers\n",
	      inst, cfg->iceserverc);
	
	debug("wcall(%p): call_config: KASE is %sabled\n",
	      inst, cfg->features.kase ? "En" : "Dis");

	msystem_enable_kase(inst->msys, cfg->features.kase);
	
	if (first && inst->readyh) {
		int ver = WCALL_VERSION_3;

		inst->readyh(ver, inst->arg);
	}
}


AVS_EXPORT
void *wcall_create(const char *userid,
		   const char *clientid,
		   wcall_ready_h *readyh,
		   wcall_send_h *sendh,
		   wcall_incoming_h *incomingh,
		   wcall_missed_h *missedh,
		   wcall_answered_h *answerh,
		   wcall_estab_h *estabh,
		   wcall_close_h *closeh,
		   wcall_metrics_h *metricsh,
		   wcall_config_req_h *cfg_reqh,
		   wcall_audio_cbr_change_h *acbrh,
		   wcall_video_state_change_h *vstateh,
		   void *arg)
{
	return wcall_create_ex(userid,
			       clientid,
			       true,
			       readyh,
			       sendh,
			       incomingh,
			       missedh,
			       answerh,
			       estabh,
			       closeh,
			       metricsh,
			       cfg_reqh,
			       acbrh,
			       vstateh,
			       arg);
}


AVS_EXPORT
void *wcall_create_ex(const char *userid,
		      const char *clientid,
		      int use_mediamgr,
		      wcall_ready_h *readyh,
		      wcall_send_h *sendh,
		      wcall_incoming_h *incomingh,
		      wcall_missed_h *missedh,
		      wcall_answered_h *answerh,
		      wcall_estab_h *estabh,
		      wcall_close_h *closeh,
		      wcall_metrics_h *metricsh,
		      wcall_config_req_h *cfg_reqh,
		      wcall_audio_cbr_change_h *acbrh,
		      wcall_video_state_change_h *vstateh,
		      void *arg)
{

	struct calling_instance *inst = NULL;
	int err;

	if (!str_isset(userid) || !str_isset(clientid))
		return NULL;

	info(APITAG "wcall: create userid=%s clientid=%s\n", userid, clientid);

	inst = mem_zalloc(sizeof(*inst), NULL);
	if (inst == NULL) {
		return NULL;
	}

	err = wcall_marshal_alloc(&inst->marshal);
	if (err) {
		warning("wcall_create: could not allocate marshal\n");
		goto out;
	}

	if (use_mediamgr != 0) {
		err = mediamgr_alloc(&inst->mm, mm_mcat_changed, inst);
		if (err) {
			warning("wcall: init: cannot allocate mediamgr inst=%p\n",
				inst);
			goto out;
		}
		debug("wcall: mediamgr=%p\n", inst->mm);
		mediamgr_register_route_change_h(inst->mm, mm_audio_route_changed,
						 inst);
	}

	err = str_dup(&inst->userid, userid);
	if (err)
		goto out;
	
	err = str_dup(&inst->clientid, clientid);
	if (err)
		goto out;

	inst->readyh = readyh;
	inst->sendh = sendh;
	inst->incomingh = incomingh;
	inst->missedh = missedh;
	inst->answerh = answerh;
	inst->estabh = estabh;
	inst->closeh = closeh;
	inst->metricsh = metricsh;
	inst->cfg_reqh = cfg_reqh;
	inst->vstateh = vstateh;
	inst->acbrh = acbrh;
	inst->arg = arg;

	inst->conf.econf.timeout_setup = 60000;
	inst->conf.econf.timeout_term  =  5000;
	inst->conf.trace = 0;
	
	err = lock_alloc(&inst->lock);
	if (err)
		goto out;

	err = msystem_get(&inst->msys, "voe", NULL);
	if (err) {
		warning("wcall(%p): create, cannot init msystem: %m\n",
			inst, err);
		goto out;
	}

	err = msystem_enable_datachannel(inst->msys, true);
	if (err) {
		warning("wcall(%p): create: enable datachannel failed (%m)\n",
			inst, err);
		goto out;
	}
	
	err = config_alloc(&inst->cfg,
			   config_req_handler,
			   config_update_handler,
			   inst);
	if (err) {
		warning("wcall(%p): create: config_alloc failed (%m)\n",
			inst, err);
		goto out;		
	}
	config_start(inst->cfg);
	
	err = async_cfg_wait(inst);

	list_append(&instances, &inst->le, inst);
out:
	if (err) {
		wcall_destroy(inst);
		return NULL;
	}

	info(APITAG "wcall: create return inst=%p\n", inst);
	return inst;

}


AVS_EXPORT
void wcall_destroy(void *id)
{
	struct calling_instance *inst = id;

	info(APITAG "wcall: destroy inst=%p\n", inst);

	if (!inst) {
		warning("wcall_destroy: no instance\n");
		return;
	}

	if (inst->thread_run) {
		inst->thread_run = false;

		debug("wcall: joining thread..\n");

		pthread_join(inst->tid, NULL);
		inst->tid = 0;
	}

	tmr_cancel(&inst->tmr_roam);
	debug("wcall: derefing marshal\n");
	inst->marshal = mem_deref(inst->marshal);
	debug("wcall: derefing marshal...DONE\n");
	

	list_flush(&inst->wcalls);	

	lock_write_get(inst->lock);
	
	list_flush(&inst->ecalls);
	list_flush(&inst->ctxl);

	inst->userid = mem_deref(inst->userid);
	inst->clientid = mem_deref(inst->clientid);
	inst->mm = mem_deref(inst->mm);
	inst->msys = mem_deref(inst->msys);
	inst->cfg = mem_deref(inst->cfg);

	inst->readyh = NULL;
	inst->sendh = NULL;
	inst->incomingh = NULL;
	inst->estabh = NULL;
	inst->closeh = NULL;
	inst->vstateh = NULL;
	inst->acbrh = NULL;
	inst->cfg_reqh = NULL;
	inst->arg = NULL;

	list_unlink(&inst->le);
	
	lock_rel(inst->lock);
	inst->lock = mem_deref(inst->lock);

	mem_deref(inst);

	debug("wcall: closed.\n");
}


AVS_EXPORT
int wcall_i_start(struct wcall *wcall, int is_video_call, int group,
		  int audio_cbr, void *extcodec_arg)
{
	int err = 0;
	struct calling_instance *inst = wcall->inst;
	bool cbr = audio_cbr != 0;
	
	info("wcall(%p): start: convid=%s video=%s audio_cbr=%s\n",
	     wcall, wcall->convid,
	     (is_video_call != 0) ? "yes" : "no",
	     cbr ? "yes" : "no");

	wcall->video.video_call = (is_video_call != 0);
	if (wcall && wcall->ecall && wcall->state != WCALL_STATE_NONE) {
		info("wcall(%p): start: found call in state '%s'\n",
		     wcall, econn_state_name(ecall_state(wcall->ecall)));
		if (ecall_state(wcall->ecall) == ECONN_PENDING_INCOMING) {
			err = ecall_set_video_send_active(wcall->ecall,
							  (bool)is_video_call);
			if (err)
				goto out;

			err = ecall_answer(wcall->ecall, cbr, extcodec_arg);
			if (!err)
				set_state(wcall, WCALL_STATE_ANSWERED);
				
			goto out;
		}
		else {
			err = EALREADY;
			goto out;
		}
	}
	else {
		set_state(wcall, WCALL_STATE_OUTGOING);

		if (group) {
			err = egcall_start(wcall->group.call, cbr);
		}
		else {
			ecall_set_video_send_active(wcall->ecall,
						    (bool)is_video_call);
			err = ecall_start(wcall->ecall, cbr, extcodec_arg);
		}
		if (err)
			goto out;
	}


    
	if (inst->mm) {
		enum mediamgr_state state;

		state = is_video_call ? MEDIAMGR_STATE_OUTGOING_VIDEO_CALL
			              : MEDIAMGR_STATE_OUTGOING_AUDIO_CALL;
		mediamgr_set_call_state(inst->mm, state);
	}
 out:
	return err;
}


int wcall_i_answer(struct wcall *wcall, int audio_cbr, void *extcodec_arg)
{
	int err = 0;
	struct calling_instance *inst;
	bool cbr = audio_cbr != 0;

	info(APITAG "wcall(%p): answer\n", wcall);

	if (!wcall)
		return EINVAL;

	inst = wcall->inst;
	if (!wcall->ecall && !wcall->group.call) {
		warning("wcall(%p): answer: no call object found\n", wcall);
		return ENOTSUP;
	}
	else if (wcall->group.call) {
		if (wcall->ecall) {
			warning("wcall(%p): answer: both ecall & egcall set, using egcall\n", wcall);
		}
		set_state(wcall, WCALL_STATE_ANSWERED);
		err = egcall_answer(wcall->group.call, cbr);
	}
	else { // ecall
		set_state(wcall, WCALL_STATE_ANSWERED);
		err = ecall_answer(wcall->ecall, cbr, extcodec_arg);
	}

	return err;
}


void wcall_i_resp(void *id, int status, const char *reason, void *arg)
{
	struct wcall_ctx *ctx = arg;
	struct calling_instance *inst = id;
	struct wcall *wcall = ctx ? ctx->context : NULL;
	struct le *le;

	info("wcall(%p): resp: status=%d reason=[%s] ctx=%p\n",
	     wcall, status, reason, ctx);
	
	lock_write_get(inst->lock);
	LIST_FOREACH(&inst->ctxl, le) {
		struct wcall_ctx *at = le->data;

		if (at == ctx) {
			goto out;
		}
	}

	warning("wcall(%p): resp: ctx:%p not found\n", wcall, ctx);
	ctx = NULL;

 out:
	lock_rel(inst->lock);
	mem_deref(ctx);
}


void wcall_i_config_update(void *id, int err, const char *json_str)
{
	struct calling_instance *inst = id;

	info("wcall(%p): config_update: err=%d json=%s\n",
	     inst, err, json_str);
	
	if (!inst)
		return;

	err = config_update(inst->cfg, err, json_str, str_len(json_str));
	if (err)
		warning("wcall(%p): config_update failed: %m\n", inst, err);
}


void wcall_i_recv_msg(void *id,
		      struct econn_message *msg,
		      uint32_t curr_time,
		      uint32_t msg_time,
		      const char *convid,
		      const char *userid,
		      const char *clientid)
{
	struct calling_instance *inst = id;
	struct wcall *wcall;
	bool handled;
	int err = 0;

	if (!inst) {
		warning("wcall_i_recv_msg: no instance\n");
		return;
	}

	wcall = wcall_lookup(id, convid);
	
	info("wcall(%p): c3_message_recv: convid=%s from=%s.%s to=%s.%s "
	     "msg=%H age=%u seconds inst=%p\n",
	     wcall, convid, userid, clientid,
	     strlen(msg->dest_userid) > 0 ? msg->dest_userid : "ALL",
	     strlen(msg->dest_clientid) > 0 ? msg->dest_clientid : "ALL",
	     econn_message_brief, msg, msg->age, inst);

	if (econn_is_creator(inst->userid, userid, msg) &&
	    (msg->age * 1000) > inst->conf.econf.timeout_setup) {
		bool is_video = false;

		if (msg->u.setup.props) {
			const char *vr;

			vr = econn_props_get(msg->u.setup.props, "videosend");
			is_video = vr ? streq(vr, "true") : false;

			if (inst->missedh) {
				uint64_t now = tmr_jiffies();
				inst->missedh(convid, msg_time,
						userid, is_video ? 1 : 0,
						inst->arg);

				info("wcall(%p): inst->missedh (%s) "
				     "took %ld ms\n",
				     wcall, is_video ? "video" : "audio",
				     tmr_jiffies() - now);
			}
		}

		return;
	}
	
	if (!wcall) {
		if (msg->msg_type == ECONN_GROUP_START
		    && econn_message_isrequest(msg)) {
			err = wcall_add(id, &wcall, convid, true);
		}
		else if (msg->msg_type == ECONN_GROUP_CHECK
		    && !econn_message_isrequest(msg)) {
			err = wcall_add(id, &wcall, convid, true);
		}
		else if (econn_is_creator(inst->userid, userid, msg)) {
			err = wcall_add(id, &wcall, convid, false);

			if (err) {
				warning("wcall(%p): recv_msg: could not add call: "
					"%m\n", wcall, err);
				goto out;
			}
		}
		else {
			err = EPROTO;
			goto out;
		}
		if (err) {
			warning("wcall(%p): recv_msg: could not add call: "
				"%m\n", wcall, err);
			goto out;
		}
	}


	handled = egcall_msg_recv(wcall->group.call,
				  curr_time, msg_time,
				  userid, clientid, msg);
	if (handled) {
		err = 0;
	}
	else {
		const char *peer_userid =
			ecall_get_peer_userid(wcall->ecall);
		const char *peer_clientid =
			ecall_get_peer_clientid(wcall->ecall);

		if (!peer_userid)
			ecall_set_peer_userid(wcall->ecall, userid);
		if (!peer_clientid)
			ecall_set_peer_clientid(wcall->ecall, clientid);

		ecall_msg_recv(wcall->ecall,
			       curr_time,
			       msg_time,
			       userid,
			       clientid,
			       msg);
	}

 out:
	return;
}


static bool wcall_has_calls(struct calling_instance *inst)
{
	struct le *le;
	
	LIST_FOREACH(&inst->wcalls, le) {
		struct wcall *wcall = le->data;

		switch(wcall->state) {
		case WCALL_STATE_NONE:
		case WCALL_STATE_TERM_LOCAL:
		case WCALL_STATE_TERM_REMOTE:
			break;

		default:
			return true;
		}
	}
	return false;
}


static void wcall_end_internal(struct wcall *wcall)
{
	info("wcall(%p): end\n", wcall);
	if (!wcall)
		return;

	if (!wcall->ecall && !wcall->group.call) {
		warning("wcall(%p): end: no call object found\n", wcall);
		return;
	}
	else if (wcall->ecall && wcall->group.call) {
		warning("wcall(%p): end: both ecall & egcall set, ending both\n", wcall);
	}

	if (wcall->group.call) {
		egcall_end(wcall->group.call);
	}
	if (wcall->ecall) {
		if (wcall->state != WCALL_STATE_TERM_REMOTE)
			set_state(wcall, WCALL_STATE_TERM_LOCAL);

		ecall_end(wcall->ecall);
	}

	if (!wcall_has_calls(wcall->inst)) {
		if (wcall->inst->mm) {
			mediamgr_set_call_state(wcall->inst->mm,
						MEDIAMGR_STATE_NORMAL);
		}
	}
}


int wcall_i_reject(struct wcall *wcall)
{
	struct calling_instance *inst;

	info(APITAG "wcall(%p): reject convid=%s\n", wcall, wcall ? wcall->convid : "");

	if (!wcall)
		return EINVAL;

	inst = wcall->inst;

	info("wcall(%p): reject: convid=%s\n", wcall, wcall->convid);

	if (!wcall_has_calls(wcall->inst)) {
		if (wcall->inst->mm) {
			mediamgr_set_call_state(wcall->inst->mm,
						MEDIAMGR_STATE_NORMAL);
		}
	}

	/* XXX Do nothing for now?
	 * Later, send REJECT message to all of your own devices
	 */

	if (inst->closeh) {
		inst->closeh(WCALL_REASON_STILL_ONGOING, wcall->convid,
			ECONN_MESSAGE_TIME_UNKNOWN, inst->userid, inst->arg);
	}

	return 0;
}


void wcall_i_end(struct wcall *wcall)
{
	info(APITAG "wcall(%p): end convid=%s\n", wcall, wcall ? wcall->convid : "");

	if (wcall)
		wcall_end_internal(wcall);
}


AVS_EXPORT
void wcall_set_data_chan_estab_handler(void *wuser,
				       wcall_data_chan_estab_h *dcestabh)
{
	struct calling_instance *inst = wuser;

	inst->dcestabh = dcestabh;
}



#if 0 /* XXX Disabled for release-3.5 (enable again for proper integration with clients) */
static bool call_restart_handler(struct le *le, void *arg)
{
	struct wcall *wcall = list_ledata(le);
	
	(void)arg;

	if (!wcall)
		return false;

	if (wcall->ecall) {
		info("wcall(%p): restarting call: %p in conv: %s\n",
		     wcall, wcall->ecall, wcall->convid);

		ecall_restart(wcall->ecall);
	}

	
	return false;
}
#endif


#if 0 /* XXX Disabled for release-3.5 (enable again for proper integration with clients) */
static void tmr_roaming_handler(void *arg)
{
	struct sa laddr;
	char ifname[64] = "";
	(void)arg;

	sa_init(&laddr, AF_INET);

	(void)net_rt_default_get(AF_INET, ifname, sizeof(ifname));
	(void)net_default_source_addr_get(AF_INET, &laddr);

	info("wcall: network_changed: %s|%j\n", ifname, &laddr);

	/* Go through all the calls, and restart flows on them */
	lock_write_get(inst->lock);
	list_apply(&inst->wcalls, true, call_restart_handler, NULL);
	lock_rel(inst->lock);
}
#endif


void wcall_i_network_changed(void)
{
	/* Reset the previous timer */
	//tmr_start(&inst->tmr_roam, 500, tmr_roaming_handler, NULL);
	info(APITAG "wcall: network_changed\n");
}


AVS_EXPORT
void wcall_set_state_handler(void *id, wcall_state_change_h *stateh)
{
	struct calling_instance *inst = id;

	info(APITAG "wcall: set_state_handler %p inst=%p\n", stateh, inst);

	if (!inst) {
		warning("wcall_set_state_handler: no instance\n");
		return;
	}

	inst->stateh = stateh;
}

void wcall_i_set_video_send_active(struct wcall *wcall, bool active)
{
	if (!wcall)
		return;

	info(APITAG "wcall(%p): set_video_send_active convid=%s active=%d\n",
	     wcall, wcall->convid, active);

	ecall_set_video_send_active(wcall->ecall, active);	
}


AVS_EXPORT
int wcall_is_video_call(void *id, const char *convid)
{
	struct wcall *wcall;

	wcall = wcall_lookup(id, convid);
	if (wcall) {
		info(APITAG "wcall(%p): is_video_call convid=%s is_video=%s\n", wcall, convid,
			wcall->video.video_call ? "yes" : "no");	
		return wcall->video.video_call;
	}

	info(APITAG "wcall(%p): is_video_call convid=%s is_video=no\n", wcall, convid);	
	return 0;
}


AVS_EXPORT
int wcall_debug(struct re_printf *pf, const void *id)
{
	struct msystem_turn_server *turnv = NULL;
	size_t turnc = 0;
	struct le *le;	
	int err = 0;

	const struct calling_instance *inst = id;

	if (!inst) {
		re_hprintf(pf, "\n");
		return 0;
	}

	turnc = msystem_get_turn_servers(&turnv, inst->msys);	
	err = re_hprintf(pf, "turnc=%zu\n", turnc);

	err = re_hprintf(pf, "# calls=%d\n", list_count(&inst->wcalls));
	LIST_FOREACH(&inst->wcalls, le) {
		struct wcall *wcall = le->data;

		err |= re_hprintf(pf, "WCALL %p in state: %s\n", wcall,
			wcall_state_name(wcall->state));
		err |= re_hprintf(pf, "convid: %s\n", wcall->convid);
		if (wcall->group.call) {
			err |= re_hprintf(pf, "\t%H\n", egcall_debug,
					  wcall->group.call);
		}
		else if (wcall->ecall) {
			err |= re_hprintf(pf, "\t%H\n", ecall_debug,
					  wcall->ecall);
		}
	}
	
	return err;
}


AVS_EXPORT
void wcall_set_trace(void *id, int trace)
{
	struct calling_instance *inst = id;

	if (!inst) {
		warning("wcall_set_trace: no instance\n");
		return;
	}

	inst->conf.trace = trace;
}


AVS_EXPORT
int wcall_get_state(void *id, const char *convid)
{
	struct wcall *wcall;
    
	wcall = wcall_lookup(id, convid);

	return wcall ? wcall->state : WCALL_STATE_UNKNOWN;
}


AVS_EXPORT
struct ecall *wcall_ecall(void *id, const char *convid)
{
	struct wcall *wcall;
    
	wcall = wcall_lookup(id, convid);
    
	return wcall ? wcall->ecall : NULL;
}


AVS_EXPORT
void wcall_propsync_request(void *id, const char *convid)
{
	int err;

	err = ecall_propsync_request(wcall_ecall(id, convid));
	if (err) {
		warning("wcall: ecall_propsync_request failed (%m)\n", err);
	}
}


AVS_EXPORT
void wcall_iterate_state(void *id, wcall_state_change_h *stateh, void *arg)	
{
	struct calling_instance *inst = id;
	struct wcall *wcall;
	struct le *le;

	if (!inst) {
		warning("wcall_iterate_state: no instance\n");
		return;
	}

	lock_write_get(inst->lock);
	LIST_FOREACH(&inst->wcalls, le) {
		wcall = le->data;

		if (wcall->state != WCALL_STATE_NONE)
			stateh(wcall->convid, wcall->state, arg);
	}
	lock_rel(inst->lock);
}


AVS_EXPORT
void wcall_set_group_changed_handler(void *id, wcall_group_changed_h *chgh,
				     void *arg)
{
	struct calling_instance *inst = id;

	info(APITAG "wcall: set_group_changed_handler %p inst=%p\n", chgh, inst);

	if (!inst) {
		warning("wcall_set_group_changed_handler: no instance\n");
		return;
	}

	inst->group.chgh = chgh;
	inst->group.arg = arg;
}


AVS_EXPORT
struct wcall_members *wcall_get_members(void *id, const char *convid)
{
	struct wcall_members *members;
	struct wcall *wcall;

	if (!id)
		return NULL;

	wcall = wcall_lookup(id, convid);

	if (!wcall || !wcall->group.call)
		return NULL;
	else {
		int err;
		
		err =  egcall_get_members(&members, wcall->group.call);

		return err ? NULL : members;
	}
}


AVS_EXPORT
void wcall_free_members(struct wcall_members *members)
{
	mem_deref(members);
}


AVS_EXPORT
void wcall_enable_privacy(void *id, int enabled)
{
	struct calling_instance *inst = id;

	info(APITAG "wcall: enable_privacy enabled=%d inst=%p\n", enabled, inst);

	if (!inst) {
		warning("wcall: enable_privacy -- no instance\n");
		return;
	}

	if (!inst->msys) {
		warning("wcall: enable_privacy -- no msystem\n");
		return;
	}

	msystem_enable_privacy(inst->msys, !!enabled);
}


const char *wcall_reason_name(int reason)
{
	switch (reason) {

	case WCALL_REASON_NORMAL:             return "Normal";
	case WCALL_REASON_ERROR:              return "Error";
	case WCALL_REASON_TIMEOUT:            return "Timeout";
	case WCALL_REASON_LOST_MEDIA:         return "LostMedia";
	case WCALL_REASON_CANCELED:           return "Canceled";
	case WCALL_REASON_ANSWERED_ELSEWHERE: return "Elsewhere";
	case WCALL_REASON_IO_ERROR:           return "I/O";
	case WCALL_REASON_STILL_ONGOING:      return "Ongoing";
	case WCALL_REASON_TIMEOUT_ECONN:      return "TimeoutEconn";
	case WCALL_REASON_DATACHANNEL:        return "DataChannel";
	case WCALL_REASON_REJECTED:           return "Rejected";

	default: return "???";
	}
}


/**
 * Return a borrowed reference to the Media-Manager
 */
struct mediamgr *wcall_mediamgr(void *id)
{
	struct calling_instance *inst = id;

	if (!inst) {
		warning("wcall_mediamgr: no instance\n");
		return NULL;
	}

	return inst->mm;
}


void wcall_handle_frame(struct avs_vidframe *frame)
{
	if (frame) {
		vie_capture_router_handle_frame(frame);
	}
}


struct wcall_marshal *wcall_get_marshal(void *wuser)
{
	struct calling_instance *inst = wuser;

	return inst ? inst->marshal : NULL;
}


static void wcall_log_handler(uint32_t level, const char *msg, void *arg)
{
	struct log_entry *loge = arg;
	int wlvl;

	switch (level) {
	case LOG_LEVEL_DEBUG:
		wlvl = WCALL_LOG_LEVEL_DEBUG;
		break;
		
	case LOG_LEVEL_INFO:
		wlvl = WCALL_LOG_LEVEL_INFO;
		break;
		
	case LOG_LEVEL_WARN:
		wlvl = WCALL_LOG_LEVEL_WARN;		
		break;

	case LOG_LEVEL_ERROR:
		wlvl = WCALL_LOG_LEVEL_ERROR;		
		break;

	default:
		wlvl = WCALL_LOG_LEVEL_ERROR;
		break;
	}
	
	
	if (loge->logh)
		loge->logh(wlvl, msg, loge->arg);
}


AVS_EXPORT
void wcall_set_log_handler(wcall_log_h *logh, void *arg)
{
	struct log_entry *loge;
		
	loge = mem_zalloc(sizeof(*loge), NULL);
	if (!loge)
		return;

	loge->logh = logh;
	loge->arg = arg;

	loge->logger.h = wcall_log_handler;
	loge->logger.arg = loge;

	log_register_handler(&loge->logger);

	list_append(&calling.logl, &loge->le, loge);
}
