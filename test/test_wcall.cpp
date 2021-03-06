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

#include <re.h>
#include <avs.h>
#include <avs_wcall.h>
#include <gtest/gtest.h>
#include "ztest.h"


struct client {
	struct le le;
	char userid[256];
	char clientid[64];
	void *wuser;
	struct odict *odict;
	int err;
	unsigned n_datachan;
	unsigned n_incoming;
	unsigned n_closed;
};


unsigned num_streams(unsigned num_clients)
{
	if (num_clients >= 2)
		return num_clients - 1;
	else
		return 0;
}


bool client_is_complete(const struct client *cli)
{
	return cli->n_datachan >= num_streams(list_count(cli->le.list));
}


bool test_is_complete(const struct client *cli)
{
	struct list *clil = cli->le.list;
	struct le *le;

	if (list_isempty(clil))
		return false;

	for (le = clil->head; le; le = le->next) {
		struct client *cli0 = (struct client *)le->data;

		if (!client_is_complete(cli0))
			return false;
	}

	return true;
}


bool clients_are_closed(const struct client *cli)
{
	struct list *clil = cli->le.list;
	struct le *le;

	if (list_isempty(clil))
		return false;

	for (le = clil->head; le; le = le->next) {
		struct client *cli0 = (struct client *)le->data;

		if (!cli0->n_closed)
			return false;
	}

	return true;
}


void test_abort(struct client *cli, int err)
{
	cli->err = err;
	re_cancel();
}


void end_all_calls(const struct client *cli, const char *convid)
{
	struct list *clil = cli->le.list;
	struct le *le;

	for (le = clil->head; le; le = le->next) {
		struct client *cli0 = (struct client *)le->data;

		wcall_end(cli0->wuser, convid);
	}
}


struct client *client_lookup(const struct list *clientl,
			     const char *userid, const char *clientid)
{
	struct le *le;

	for (le = clientl->head; le; le = le->next) {

		struct client *cli = (struct client *)le->data;

		if (0 == str_casecmp(cli->userid, userid) &&
		    0 == str_casecmp(cli->clientid, clientid)) {

			return cli;
		}
	}

	return NULL;
}


static int send_handler(void *ctx, const char *convid,
			const char *userid_self, const char *clientid_self,
			const char *userid_dest, const char *clientid_dest,
			const uint8_t *data, size_t len, int transient,
			void *arg)
{
	struct client *cli = (struct client *)arg;
	struct list *clientl = cli->le.list;
	struct le *le;

	info("[ %s.%s ] {%s} send message from %s.%s ---> %s.%s\n",
		  cli->userid, cli->clientid,
		  convid,
		  userid_self, clientid_self,
		  userid_dest, clientid_dest);

	/* The data payload is a Calling-3 JSON message */

	if (str_isset(userid_dest) && str_isset(clientid_dest)) {

		struct client *cli_target;

		cli_target = client_lookup(clientl,
					   userid_dest, clientid_dest);
		if (cli_target) {
			wcall_recv_msg(cli_target->wuser, data, len,
				       0,
				       0,
				       convid,
				       userid_self,
				       clientid_self);
		}
		else {
			warning("client not found\n");
		}
	}
	else {
		/* Forward the message to all clients, except itself */
		for (le = clientl->head; le; le = le->next) {

			struct client *cli_target = (struct client *)le->data;

			if (0 == str_casecmp(cli_target->userid, userid_self))
				continue;

			wcall_recv_msg(cli_target->wuser, data, len,
				       0,
				       0,
				       convid,
				       userid_self,
				       clientid_self);
		}
	}


	wcall_resp(cli->wuser, 200, "", ctx);

	return 0;
}


static void incoming_handler(const char *convid, uint32_t msg_time,
			     const char *userid, int video_call /*bool*/,
			     int should_ring /*bool*/,
			     void *arg)
{
	struct client *cli = (struct client *)arg;
	int err;

	info("[ %s.%s ] {%s} incoming call from %s\n",
		  cli->userid, cli->clientid,
		  convid,
		  userid);

	++cli->n_incoming;

	/* verify parameters */
	ASSERT_TRUE(str_isset(convid));
	ASSERT_STREQ("A", userid);

#if 1
	/* Auto-answer */
	err = wcall_answer(cli->wuser, convid, 0);
	ASSERT_EQ(0, err);
#endif

}


static void answered_handler(const char *convid, void *arg)
{
	struct client *cli = (struct client *)arg;

	re_printf("[ %s.%s ] {%s} call answered\n",
		  cli->userid, cli->clientid,
		  convid);
}


static void estab_handler(const char *convid,
			  const char *userid, void *arg)
{
	struct client *cli = (struct client *)arg;

	re_printf("[ %s.%s ] {%s} call established with \"%s\"\n",
		  cli->userid, cli->clientid,
		  convid, userid);
}


static void close_handler(int reason, const char *convid, uint32_t msg_time,
			  const char *userid, void *arg)
{
	struct client *cli = (struct client *)arg;

	warning("[ %s.%s ] {%s} call closed (%s)\n",
		cli->userid, cli->clientid,
		convid,
		wcall_reason_name(reason));

	++cli->n_closed;

	if (clients_are_closed(cli)) {
		re_printf("all calls closed\n");
		re_cancel();
	}

	test_abort(cli, ECONNRESET);
}


static void datachan_estab_handler(const char *convid,
				   const char *userid, void *arg)
{
	struct client *cli = (struct client *)arg;

	info("[ %s.%s ] {%s} data channel established (%s)\n",
		  cli->userid, cli->clientid,
		  convid, userid);

	++cli->n_datachan;

	if (0 == str_casecmp(cli->userid, userid)) {
		test_abort(cli, EPROTO);
		return;
	}

	if (!cli->odict)
		odict_alloc(&cli->odict, 32);

	if (!odict_lookup(cli->odict, userid))
		odict_entry_add(cli->odict, userid, ODICT_STRING, userid);

	if (test_is_complete(cli)) {
		info("test complete\n");
		//end_all_calls(cli, convid);
		re_cancel();
	}

	return;

 error:
	test_abort(cli, EPROTO);
}


static void client_destructor(void *data)
{
	struct client *cli = (struct client *)data;

	mem_deref(cli->odict);
	wcall_destroy(cli->wuser);
}


void client_alloc(struct client **clip, struct list *lst,
		  const char *userid, const char *clientid)
{
	struct client *cli;
	const bool use_mediamgr = false;

	cli = (struct client *)mem_zalloc(sizeof(*cli), client_destructor);

	str_ncpy(cli->userid, userid, sizeof(cli->userid));
	str_ncpy(cli->clientid, clientid, sizeof(cli->clientid));

	cli->wuser = wcall_create_ex(userid,
				     clientid,
				     use_mediamgr,
				     NULL,
				     send_handler,
				     incoming_handler,
				     0,
				     answered_handler,
				     estab_handler,
				     close_handler,
				     0,
				     0,
				     0,
				     0,
				     cli);
	ASSERT_TRUE(cli->wuser != NULL);

	wcall_set_data_chan_estab_handler(cli->wuser, datachan_estab_handler);

	list_append(lst, &cli->le, cli);
	*clip = cli;

	info("test: new client %s.%s\n", userid, clientid);
}


TEST(wcall, double_close)
{
	int err;

	err = wcall_init();
	ASSERT_EQ(0, err);

	wcall_close();

	/* Extra close, should not crash! */
	wcall_close();
}


TEST(wcall, only_close)
{
	wcall_close();
	wcall_close();
	wcall_close();
	wcall_close();
}


TEST(wcall, create_multiple)
{
	int err;

	err = wcall_init();
	ASSERT_EQ(0, err);

	void *wuser1 = wcall_create("abc", "123", NULL, NULL, NULL, NULL, NULL,
		NULL, NULL, NULL, NULL, NULL, NULL, NULL);

	void *wuser2 = wcall_create("abd", "234", NULL, NULL, NULL, NULL, NULL,
		NULL, NULL, NULL, NULL, NULL, NULL, NULL);


	wcall_destroy(wuser1);
	wcall_destroy(wuser2);

	wcall_close();
}


#define NUM_CLIENTS 7


class Wcall : public ::testing::Test {

public:
	virtual void SetUp() override
	{
		int err;

		/* This is needed for multiple-calls test */
		err = ztest_set_ulimit(512);
		ASSERT_EQ(0, err);

		err = flowmgr_init("audummy");
		ASSERT_EQ(0, err);

		msystem_enable_kase(flowmgr_msystem(), true);

		err = wcall_init();
		ASSERT_EQ(0, err);
	}

	virtual void TearDown() override
	{
		wcall_close();

		flowmgr_close();
	}

public:
};


TEST_F(Wcall, group_call_start_all)
{
	struct client *cliv[NUM_CLIENTS];
	static const char *convid = "00cc";
	struct list clientl = LIST_INIT;
	const int is_group = 1;
	unsigned i;
	int err;

#if 0
	log_set_min_level(LOG_LEVEL_INFO);
	log_enable_stderr(true);
#endif

	/* create all the clients */
	for (i=0; i<ARRAY_SIZE(cliv); i++) {

		char userid[2]   = { (char)('A' + i), '\0'};
		char clientid[2] = { (char)('1' + i), '\0'};

		client_alloc(&cliv[i], &clientl, userid, clientid);
	}

	/* Start a Group-call from all the clients */
	for (i=0; i<ARRAY_SIZE(cliv); i++) {

		err = wcall_start(cliv[i]->wuser, convid, 0, is_group, 0);
		ASSERT_EQ(0, err);
	}

	err = re_main_wait(60000);
	ASSERT_EQ(0, err);

	for (i=0; i<ARRAY_SIZE(cliv); i++) {

		ASSERT_EQ(0, cliv[i]->err);
		ASSERT_EQ(num_streams(NUM_CLIENTS), cliv[i]->n_datachan);
		ASSERT_EQ(0, cliv[i]->n_incoming);
		ASSERT_EQ(0, cliv[i]->n_closed);

		ASSERT_EQ(num_streams(NUM_CLIENTS),
			  odict_count(cliv[i]->odict, false));
	}

	/* destroy all the clients */
	for (i=0; i<ARRAY_SIZE(cliv); i++) {

		mem_deref(cliv[i]);
	}
}


TEST_F(Wcall, group_call_answer)
{
	struct client *cliv[NUM_CLIENTS];
	static const char *convid = "00cc";
	struct list clientl = LIST_INIT;
	const int is_group = 1;
	unsigned i;
	int err;

#if 0
	log_set_min_level(LOG_LEVEL_INFO);
	log_enable_stderr(true);
#endif

	/* create all the clients */
	for (i=0; i<ARRAY_SIZE(cliv); i++) {

		char userid[2]   = { (char)('A' + i), '\0'};
		char clientid[2] = { (char)('1' + i), '\0'};

		client_alloc(&cliv[i], &clientl, userid, clientid);
	}

	/* Start a Group-call from the first client */
	err = wcall_start(cliv[0]->wuser, convid, 0, is_group, 0);
	ASSERT_EQ(0, err);

	err = re_main_wait(60000);
	ASSERT_EQ(0, err);

	for (i=0; i<ARRAY_SIZE(cliv); i++) {

		ASSERT_EQ(0, cliv[i]->err);
		ASSERT_EQ(num_streams(NUM_CLIENTS), cliv[i]->n_datachan);

		if (i==0) {
			ASSERT_EQ(0, cliv[i]->n_incoming);
		}
		else {
			ASSERT_EQ(1, cliv[i]->n_incoming);
		}

		ASSERT_EQ(0, cliv[i]->n_closed);

		ASSERT_EQ(num_streams(NUM_CLIENTS),
			  odict_count(cliv[i]->odict, false));
	}

	/* destroy all the clients */
	for (i=0; i<ARRAY_SIZE(cliv); i++) {

		mem_deref(cliv[i]);
	}
}
