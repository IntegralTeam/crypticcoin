#!/usr/bin/env python2
# Copyright (c) 2019 The Crypticcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
#
# Test dPoS p2p messages
#

import os
import sys
import time
from time import sleep
from dpos_base import dPoS_BaseTest
from test_framework.util import \
    assert_equal, \
    assert_not_equal, \
    assert_greater_than, \
    connect_nodes_bi


class dPoS_p2pMessagesTest(dPoS_BaseTest):
    def print_messages(self):
        vblocks_len = [len(node.listdposviceblocks()) for node in self.nodes]
        rdvotes_len = [len(node.listdposroundvotes()) for node in self.nodes]
        txvotes_len = [len(node.listdpostxvotes()) for node in self.nodes]
        c_txs_len = [len(node.list_instant_transactions()) for node in self.nodes]
        txs_len = [len(node.getrawmempool()) for node in self.nodes]
        blocks = [node.getblockcount() for node in self.nodes]
        print("Vice-blocks:  ", vblocks_len)
        print("Round votes:  ", rdvotes_len)
        print("Tx votes:     ", txvotes_len)
        print("Txs:          ", txs_len)
        print("Committed txs:", c_txs_len)
        print("Num of blocks:", blocks)
        print("")

    def run_test(self):
        super(dPoS_p2pMessagesTest, self).run_test()
        mns = self.create_masternodes([0, 1, 2, 3, 4])
        self.stop_nodes()
        self.start_masternodes([["-reindex", "-txindex"]] * self.num_nodes)
        # First group (4 masternodes)
        connect_nodes_bi(self.nodes, 0, 1)
        connect_nodes_bi(self.nodes, 1, 2)
        connect_nodes_bi(self.nodes, 2, 3)
        connect_nodes_bi(self.nodes, 3, 4)
        connect_nodes_bi(self.nodes, 4, 0)
        # Second group (0 masternodes)
        connect_nodes_bi(self.nodes, 5, 6)
        connect_nodes_bi(self.nodes, 6, 7)
        connect_nodes_bi(self.nodes, 7, 8)
        connect_nodes_bi(self.nodes, 8, 9)
        connect_nodes_bi(self.nodes, 9, 5)
        # self.sync_nodes(0, 5)
        # self.sync_nodes(5, 10)

        print("started up")
        for i in range(4):
            self.print_messages()
            time.sleep(1)
        print("-------------------")


        print("created 2 interfering instant txs")
        [assert_equal(len(node.listdposviceblocks()), 0) for node in self.nodes]
        [assert_equal(len(node.listdposroundvotes()), 0) for node in self.nodes]
        [assert_equal(len(node.listdpostxvotes()), 0) for node in self.nodes]
        u = self.nodes[1].listunspent()
        tx1 = self.create_transaction_withInput(1, 0, u[0], self.nodes[9].getnewaddress(), 4.4, True)
        tx2 = self.create_transaction_withInput(1, 3, u[0], self.nodes[3].getnewaddress(), 4.4, True)
        for i in range(8):
            self.print_messages()
            time.sleep(1)
        print("-------------------")

        txs1 = self.nodes[2].list_instant_transactions()
        txs2 = self.nodes[7].list_instant_transactions()
        # assert_equal(len(txs1), 1)
        # assert_equal(len(txs2), 0)
        # assert_equal(txs1[0]["hash"], tx1)


        # self.sync_nodes(0, 5)
        # self.sync_nodes(5, 10)
        # vblocks = [node.listdposviceblocks() for node in self.nodes]
        # rdvotes = [node.listdposroundvotes() for node in self.nodes]
        # txvotes = [node.listdpostxvotes() for node in self.nodes]
        # sys.stdin.readline()
        # vblocks_left = vblocks[0:len(vblocks)/2]
        # vblocks_right = vblocks[len(vblocks)/2:]
        # assert_equal(len(vblocks_left), len(vblocks_right))
        # rdvotes_left = rdvotes[0:len(rdvotes)/2]
        # rdvotes_right = rdvotes[len(rdvotes)/2:]
        # assert_equal(len(rdvotes_left), len(rdvotes_right))
        # txvotes_left = vblocks[0:len(txvotes)/2]
        # txvotes_right = vblocks[len(rdvotes)/2:]
        # assert_equal(len(txvotes_left), len(txvotes_right))
        # assert_not_equal(vblocks_left, vblocks_right)
        # assert_not_equal(rdvotes_left, rdvotes_right)
        # assert_not_equal(txvotes_left, txvotes_right)
        # [assert_not_equal(len(x), 0) for x in vblocks_left]
        # [assert_not_equal(len(x), 0) for x in vblocks_right]
        # [assert_not_equal(len(x), 0) for x in rdvotes_left]
        # [assert_equal(len(x), 0) for x in rdvotes_right]
        # [assert_not_equal(len(x), 0) for x in txvotes_left]
        # [assert_not_equal(len(x), 0) for x in txvotes_right]


        print("merge 2 groups")
        connect_nodes_bi(self.nodes, 0, 6)
        for i in range(8):
            self.print_messages()
            time.sleep(1)

        print("generate 2 vblocks")
        self.nodes[3].generate(1)
        self.nodes[8].generate(1)
        for i in range(4):
            self.print_messages()
            time.sleep(1)
        print("-------------------")

        print("generate 2 vblocks")
        self.nodes[3].generate(1)
        self.nodes[8].generate(1)
        for i in range(4):
            self.print_messages()
            time.sleep(1)
        print("-------------------")


        for i in range(14):
	        print("created 2 instant txs")
	        tx1 = self.create_transaction(1, self.nodes[9].getnewaddress(), 4.4, True)
	        tx2 = self.create_transaction(6, self.nodes[0].getnewaddress(), 4.4, True)
	        for i in range(4):
	            self.print_messages()
	            time.sleep(1)
	        print("-------------------")
	        print("-------------------")

        print(self.nodes[0].listtransactions())

        # print("created 2 interfering instant txs")
        # u = self.nodes[1].listunspent()
        # tx1 = self.create_transaction_withInput(1, 0, u[0], self.nodes[9].getnewaddress(), 4.4, True)
        # tx2 = self.create_transaction_withInput(1, 3, u[0], self.nodes[3].getnewaddress(), 4.4, True)
        # for i in range(8):
        #     self.print_messages()
        #     time.sleep(1)
        # print("-------------------")

        print("generate 2 vblocks")
        self.nodes[3].generate(1)
        self.nodes[8].generate(1)
        for i in range(4):
            self.print_messages()
            time.sleep(1)
        print("-------------------")

        print(self.nodes[0].listtransactions())

if __name__ == '__main__':
    dPoS_p2pMessagesTest().main()
