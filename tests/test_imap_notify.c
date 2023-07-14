/*
 * LBBS -- The Lightweight Bulletin Board System
 *
 * Copyright (C) 2023, Naveen Albert
 *
 * Naveen Albert <bbs@phreaknet.org>
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License Version 2. See the LICENSE file
 * at the top of the source tree.
 */

/*! \file
 *
 * \brief IMAP NOTIFY Tests
 *
 * \author Naveen Albert <bbs@phreaknet.org>
 */

#include "test.h"

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

static int pre(void)
{
	test_preload_module("mod_mail.so");
	test_preload_module("mod_mimeparse.so");
	test_load_module("net_smtp.so");
	test_load_module("net_imap.so");
	test_load_module("mod_mail_events.so");

	TEST_ADD_CONFIG("mod_mail.conf");
	TEST_ADD_CONFIG("net_smtp.conf");
	TEST_ADD_CONFIG("net_imap.conf");
	TEST_ADD_CONFIG("mod_mail_events.conf");

	system("rm -rf " TEST_MAIL_DIR); /* Purge the contents of the directory, if it existed. */
	mkdir(TEST_MAIL_DIR, 0700); /* Make directory if it doesn't exist already (of course it won't due to the previous step) */
	return 0;
}

static int send_count = 0;

static int send_message(int client1, size_t extrabytes)
{
	char subject[32];

	if (!send_count++) {
		CLIENT_EXPECT(client1, "220");
		SWRITE(client1, "EHLO " TEST_EXTERNAL_DOMAIN ENDL);
		CLIENT_EXPECT_EVENTUALLY(client1, "250 "); /* "250 " since there may be multiple "250-" responses preceding it */
	} else {
		SWRITE(client1, "RSET" ENDL);
		CLIENT_EXPECT(client1, "250");
	}

	SWRITE(client1, "MAIL FROM:<" TEST_EMAIL_EXTERNAL ">\r\n");
	CLIENT_EXPECT(client1, "250");
	SWRITE(client1, "RCPT TO:<" TEST_EMAIL ">\r\n");
	CLIENT_EXPECT(client1, "250");
	SWRITE(client1, "DATA\r\n");
	CLIENT_EXPECT(client1, "354");

	snprintf(subject, sizeof(subject), "Subject: Message %d" ENDL, send_count);

	SWRITE(client1, "Date: Sun, 1 Jan 2023 05:33:29 -0700" ENDL);
	SWRITE(client1, "From: " TEST_EMAIL_EXTERNAL ENDL);
	write(client1, subject, strlen(subject));
	SWRITE(client1, "To: " TEST_EMAIL ENDL);
	SWRITE(client1, "Content-Type: text/plain" ENDL);
	SWRITE(client1, ENDL);
	SWRITE(client1, "This is a test email message." ENDL);
	SWRITE(client1, "....Let's hope it gets delivered properly." ENDL); /* Test byte stuffing */
	if (extrabytes) {
		extrabytes = MIN(sizeof(subject), extrabytes);
		memset(subject, 'a', extrabytes);
		write(client1, subject, extrabytes);
		SWRITE(client1, ENDL);
	}
	SWRITE(client1, "." ENDL); /* EOM */
	CLIENT_EXPECT(client1, "250");
	return 0;

cleanup:
	return -1;
}

static int make_messages(int nummsg)
{
	int clientfd;

	clientfd = test_make_socket(25);
	if (clientfd < 0) {
		return -1;
	}

	/* First, dump some messages into the mailbox for us to retrieve */
	while (send_count < nummsg) {
		send_message(clientfd, 0);
	}
	close(clientfd); /* Close SMTP connection */

	return 0;
}

/* Do not change this value, as the value is used hardcoded in several places that would need to be updated as well */
#define TARGET_MESSAGES 10

static int run(void)
{
	int client1 = -1, client2 = -1;
	int res = -1;

	if (make_messages(TARGET_MESSAGES)) {
		return -1;
	}
	/* Verify that the email messages were all sent properly. */
	DIRECTORY_EXPECT_FILE_COUNT(TEST_MAIL_DIR "/1/new", send_count);

	client1 = test_make_socket(143);
	if (client1 < 0) {
		return -1;
	}

	client2 = test_make_socket(143);
	if (client2 < 0) {
		return -1;
	}

	/* Connect and log in */
	CLIENT_EXPECT(client2, "OK");
	SWRITE(client2, "a1 LOGIN \"" TEST_USER "\" \"" TEST_PASS "\"" ENDL);
	CLIENT_EXPECT(client2, "a1 OK");

	SWRITE(client2, "a2 SELECT INBOX" ENDL);
	CLIENT_EXPECT_EVENTUALLY(client2, "a2 OK");

	CLIENT_EXPECT(client1, "OK");
	SWRITE(client1, "a1 LOGIN \"" TEST_USER "\" \"" TEST_PASS "\"" ENDL);
	CLIENT_EXPECT(client1, "a1 OK");

	/* NOTIFY with unsupported events */
	SWRITE(client1, "a2 NOTIFY SET (personal (FlagChange Foobar))" ENDL);
	CLIENT_EXPECT(client1, "a2 NO"); /* RFC 5465 3.1 */

	/* NOTIFY with improper event combination */
	SWRITE(client1, "a3 NOTIFY SET (personal (MessageExpunge))" ENDL);
	CLIENT_EXPECT(client1, "a3 BAD");
	SWRITE(client1, "a4 NOTIFY SET (personal (FlagChange))" ENDL);
	CLIENT_EXPECT(client1, "a4 BAD");

	/* Nonexistent mailbox */
	SWRITE(client1, "a5 NOTIFY SET (foobar (MessageNew MessageExpunge))" ENDL);
	CLIENT_EXPECT(client1, "a5 NO");

	/* NOTIFY without a selected mailbox */
	SWRITE(client1, "a6 NOTIFY SET (SELECTED (MessageNew (FLAGS) MessageExpunge))" ENDL);
	CLIENT_EXPECT(client1, "a6 OK");

	SWRITE(client1, "c1 SELECT INBOX" ENDL);
	CLIENT_EXPECT_EVENTUALLY(client1, "c1 OK");

	/* If SELECTED is used (instead of SELECTED-DELAYED), sequence numbers cannot be used (UID commands must be used) */
	SWRITE(client1, "c2 FETCH 1 (FLAGS)" ENDL);
	CLIENT_EXPECT(client1, "c2 BAD");

	/* NOTIFY with a selected mailbox */
	SWRITE(client1, "c3 NOTIFY SET STATUS (SELECTED-DELAYED (MessageNew (FLAGS) MessageExpunge FlagChange)) (personal (MessageNew (FLAGS) MessageExpunge MailboxName FlagChange))" ENDL);
	CLIENT_EXPECT(client1, "* STATUS"); /* If NOTIFY SET STATUS for different mailboxes, we should get a STATUS for those */

	/* Test FlagChange in current mailbox: should get FETCH */
	SWRITE(client1, "c4 IDLE" ENDL);
	CLIENT_EXPECT_EVENTUALLY(client1, "+ idling"); /* Expect eventually, due to previous multiline response */

	SWRITE(client2, "c5 STORE 1 +FLAGS.SILENT (\\Seen)" ENDL);
	CLIENT_EXPECT(client2, "c5 OK");

	CLIENT_EXPECT_EVENTUALLY(client1, "* 1 FETCH"); /* Untagged FETCH */

	/* If a new message arrives, we should get both an untagged EXISTS and an untagged FETCH */
	SWRITE(client2, "c6 APPEND INBOX (\\Seen) {326}" ENDL);
	CLIENT_EXPECT(client2, "+");
	SWRITE(client2, "Date: Mon, 7 Feb 1994 21:52:25 -0800 (PST)" ENDL);
	SWRITE(client2, "From: Fred Foobar <foobar@Blurdybloop.example>" ENDL);
	SWRITE(client2, "Subject: afternoon meeting" ENDL);
	SWRITE(client2, "To: mooch@owatagu.siam.edu.example" ENDL);
	SWRITE(client2, "Message-Id: <B27397-0100000@Blurdybloop.example>" ENDL);
	SWRITE(client2, "MIME-Version: 1.0" ENDL);
	SWRITE(client2, "Content-Type: TEXT/PLAIN; CHARSET=US-ASCII" ENDL);
	SWRITE(client2, ENDL);
	SWRITE(client2, "Hello Joe, do you think we can meet at 3:30 tomorrow?" ENDL);
	SWRITE(client2, ENDL);
	CLIENT_EXPECT_EVENTUALLY(client2, "11] APPEND"); /* The UID of this message should be 11 */

	CLIENT_EXPECT_EVENTUALLY(client1, "FLAGS (\\Seen)"); /* Should get an untagged FETCH response */

	SWRITE(client1, "DONE" ENDL);
	CLIENT_EXPECT(client1, "c4 OK");

	/* Test FlagChange in different mailbox: should get STATUS.
	 * Even though we're not using CONDSTORE/QRESYNC, we should get a STATUS, because UNSEEN will change. */
	SWRITE(client1, "d1 SELECT Sent" ENDL);
	CLIENT_EXPECT_EVENTUALLY(client1, "d1 OK");

	SWRITE(client1, "d2 IDLE" ENDL);
	CLIENT_EXPECT(client1, "+ idling");

	SWRITE(client2, "d3 STORE 2 +FLAGS.SILENT (\\Seen)" ENDL);
	CLIENT_EXPECT_EVENTUALLY(client2, "d3 OK");

	CLIENT_EXPECT(client1, "* STATUS");

	/* If a new message arrives, we should get a STATUS (since not currently selected) */
	SWRITE(client2, "d4 APPEND INBOX (\\Seen) {326}" ENDL);
	CLIENT_EXPECT_EVENTUALLY(client2, "+");
	SWRITE(client2, "Date: Mon, 7 Feb 1994 21:52:25 -0800 (PST)" ENDL);
	SWRITE(client2, "From: Fred Foobar <foobar@Blurdybloop.example>" ENDL);
	SWRITE(client2, "Subject: afternoon meeting" ENDL);
	SWRITE(client2, "To: mooch@owatagu.siam.edu.example" ENDL);
	SWRITE(client2, "Message-Id: <B27397-0100000@Blurdybloop.example>" ENDL);
	SWRITE(client2, "MIME-Version: 1.0" ENDL);
	SWRITE(client2, "Content-Type: TEXT/PLAIN; CHARSET=US-ASCII" ENDL);
	SWRITE(client2, ENDL);
	SWRITE(client2, "Hello Joe, do you think we can meet at 3:30 tomorrow?" ENDL);
	SWRITE(client2, ENDL);
	CLIENT_EXPECT_EVENTUALLY(client2, "12] APPEND"); /* The UID of this message should be 12 */

	CLIENT_EXPECT(client1, "* STATUS");

	/* Mailbox creation/deletion/rename */
	SWRITE(client2, "d5 CREATE foobar" ENDL);
	CLIENT_EXPECT_EVENTUALLY(client2, "d5 OK"); /* will be preceded by an untagged EXISTS */
	CLIENT_EXPECT_EVENTUALLY(client1, "\"foobar\""); /* untagged LIST response */

	/* Rename */
	SWRITE(client2, "d6 RENAME foobar foobar2" ENDL);
	CLIENT_EXPECT(client2, "d6 OK");
	CLIENT_EXPECT(client1, "OLDNAME");

	/* Delete */
	SWRITE(client2, "d7 DELETE foobar2" ENDL);
	CLIENT_EXPECT(client2, "d7 OK");
	CLIENT_EXPECT(client1, "NonExistent");

	SWRITE(client1, "DONE" ENDL);
	CLIENT_EXPECT(client1, "d2 OK");

	/* LOGOUT */
	SWRITE(client2, "z998 LOGOUT" ENDL);
	CLIENT_EXPECT(client2, "* BYE");
	SWRITE(client1, "z999 LOGOUT" ENDL);
	CLIENT_EXPECT(client1, "* BYE");
	res = 0;

cleanup:
	close_if(client1);
	close_if(client2);
	return res;
}

TEST_MODULE_INFO_STANDARD("IMAP NOTIFY Tests");
