#!/usr/bin/env python3
# Copyright (c) 2016 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test RPC commands for signing and verifying messages."""

from test_framework.test_framework import WagerrTestFramework
from test_framework.util import assert_equal

class SignMessagesTest(WagerrTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 1

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def run_test(self):
        message = 'This is just a test message'

        self.log.info('test signing with priv_key')
        priv_key = 'THTeyaP8QLTG8zwG1AdYrnWqCaaAjbj7TcW9xRhJ7n6LRLCeg6Bc'
        address = 'TPEdK89Rwds4rxdbBApYCKM6AQPcDZf8qh'
        expected_signature = 'IBT84y/5SuwQhW6FUfYfa5FwDAuTQ+JrN2+IQFJPHzUMfezt8F1SQGeNvBXzKBOluGQ8LQiF2Vlh6aGwOP9whqQ='
        signature = self.nodes[0].signmessagewithprivkey(priv_key, message)
        assert_equal(expected_signature, signature)
        assert self.nodes[0].verifymessage(address, signature, message)

        self.log.info('test signing with an address with wallet')
        address = self.nodes[0].getnewaddress()
        signature = self.nodes[0].signmessage(address, message)
        assert self.nodes[0].verifymessage(address, signature, message)

        self.log.info('test verifying with another address should not work')
        other_address = self.nodes[0].getnewaddress()
        other_signature = self.nodes[0].signmessage(other_address, message)
        assert not self.nodes[0].verifymessage(other_address, signature, message)
        assert not self.nodes[0].verifymessage(address, other_signature, message)

if __name__ == '__main__':
    SignMessagesTest().main()
