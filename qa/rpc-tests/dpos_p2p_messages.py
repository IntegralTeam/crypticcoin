#!/usr/bin/env python2
# Copyright (c) 2019 The Crypticcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
#
# Test dPoS p2p messages
#

import sys
import time
from collections import OrderedDict
from dpos_base import dPoS_BaseTest
from test_framework.util import \
    assert_equal, \
    assert_greater_than

class dPoS_P2P(dPoS_BaseTest):

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
        # 
        # Check that tx votes propagate through the network 
        #
    	self.options.node_garaph_layout = "3"
        super(dPoS_P2P, self).run_test()
        print("Creating masternodes")
        self.create_masternodes([0, 1, 2, 3, 4, 5])

     #    print("Disconnect 9th node")
    	# self.options.node_garaph_layout = "split"
     #    self.stop_nodes()
     #    self.start_masternodes([["-reindex", "-txindex"]] * self.num_nodes)
     #    self.connect_nodes()
     #    time.sleep(3)

        blockCount = self.nodes[0].getblockcount()

        time.sleep(3)
        print("Create vice-block by node 0")
        self.nodes[0].generate(1)
        time.sleep(3)
    	self.print_messages()

        print("Create 3 instant txs")
        for n in range(3):
        	self.create_transaction(n, self.nodes[n + 1].getnewaddress(), 25, True)
        time.sleep(6)
    	self.print_messages()

        print("Create vice-block by node 0")
        self.nodes[0].generate(1)

        print("Print how messages propagate through the network. 9-th node shouldn't get any messages, all other nodes should sync")
        for i in range(4):
        	self.print_messages()
        	time.sleep(1)

        # 9th node is disconnected
        #for n in range(self.num_nodes):
        	# if n == 9:
	        # 	assert_equal(len(self.nodes[n].list_instant_transactions()), 0)
	        # 	assert_equal(len(self.nodes[n].listdpostxvotes()), 0)
	        # else:
	        # 	assert_equal(len(self.nodes[n].list_instant_transactions()), 3)
	        # 	assert_equal(len(self.nodes[n].listdpostxvotes()), 12)
        	# assert_equal(len(self.nodes[n].listdposviceblocks()), 0)
        	# assert_equal(len(self.nodes[n].listdposroundvotes()), 0)

    	return

        print("Restart nodes and connect 9th node")
    	self.options.node_garaph_layout = "3"
        self.stop_nodes()
        self.start_masternodes([["-txindex"]] * self.num_nodes)

        print("After restart")
    	time.sleep(1)
        for i in range(1):
        	self.print_messages()

        self.connect_nodes()

        print("Print how messages propagate through the network. All the nodes should sync")
        for i in range(12):
        	self.print_messages()
        	time.sleep(1)

        # 9th node is now connected
        # for n in range(self.num_nodes):
        # 	assert_equal(len(self.nodes[n].list_instant_transactions()), 3)
        # 	assert_equal(len(self.nodes[n].listdposviceblocks()), 0)
        # 	assert_equal(len(self.nodes[n].listdposroundvotes()), 0)
        # 	assert_equal(len(self.nodes[n].listdpostxvotes()), 12)

        # 
        # Check that vice blocks propagate through the network 
        #
        print("Disconnect 9th node")
    	self.options.node_garaph_layout = "split"
        self.stop_nodes()
        self.start_masternodes([["-txindex"]] * self.num_nodes)
        self.connect_nodes()
        time.sleep(8)

        print("Generate vice-block on 9th node")
        self.nodes[9].generate(1)

        print("Print how messages propagate through the network. 9th shouldn't send any messages, all other nodes should sync")
        for i in range(3):
        	self.print_messages()
        	time.sleep(1)

        # 9th node is disconnected
        #for n in range(self.num_nodes):
      #   	if n == 9:
      #   		assert_equal(len(self.nodes[n].listdposviceblocks()), 1)
	     #    else:
      #   		assert_equal(len(self.nodes[n].listdposviceblocks()), 0)
    		# assert_equal(len(self.nodes[n].list_instant_transactions()), 3)
      #   	assert_equal(len(self.nodes[n].listdpostxvotes()), 12)
      #   	assert_equal(len(self.nodes[n].listdposroundvotes()), 0)

        print("Restart nodes and connect 9th node")
    	self.options.node_garaph_layout = "3"
        self.stop_nodes()
        self.start_masternodes([["-txindex"]] * self.num_nodes)

        print("After restart")
    	time.sleep(1)
        for i in range(1):
        	self.print_messages()

        self.connect_nodes()

        print("Print how messages propagate through the network. All the nodes should sync")
        for i in range(8):
        	self.print_messages()
        	time.sleep(1)

        # 9th node is now connected
        # for n in range(self.num_nodes):
        # 	assert_equal(len(self.nodes[n].list_instant_transactions()), 0)
        # 	assert_equal(len(self.nodes[n].listdposviceblocks()), 1)
        # 	assert_equal(len(self.nodes[n].listdposroundvotes()) >= 3, True)
        # 	assert_equal(len(self.nodes[n].listdpostxvotes()), 12)
        # 	assert_equal(self.nodes[n].getblockcount(), blockCount + 1)

        # 
        # Check that round votes propagate through the network 
        #
        print("Disconnect 9th node")
    	self.options.node_garaph_layout = "split"
        self.stop_nodes()
        self.start_masternodes([["-txindex"]] * self.num_nodes)
        self.connect_nodes()
        time.sleep(8)

        print("Generate vie-block on 0 node")
        self.nodes[0].generate(1)

        print("Print how messages propagate through the network. 9th shouldn't get any messages, all other nodes should sync")
        for i in range(5):
        	self.print_messages()
        	time.sleep(1)

        # 9th node is disconnected
        # for n in range(self.num_nodes):
        # 	if n == 9:
	       #  	assert_equal(len(self.nodes[n].list_instant_transactions()), 0)
        # 		assert_equal(len(self.nodes[n].listdposviceblocks()), 1)
	       #  	assert_equal(len(self.nodes[n].listdposroundvotes()) >= 3, True)
	       #  else:
	       #  	assert_equal(len(self.nodes[n].list_instant_transactions()), 0)
        # 		assert_equal(len(self.nodes[n].listdposviceblocks()), 2)
	       #  	assert_equal(len(self.nodes[n].listdposroundvotes()) >= 6, True)
        # 	assert_equal(len(self.nodes[n].listdpostxvotes()), 12)

        print("Restart nodes and connect 9th node")
    	self.options.node_garaph_layout = "3"
        self.stop_nodes()
        self.start_masternodes([["-txindex", "-debug"]] * self.num_nodes)

        print("After restart")
    	time.sleep(1)
        for i in range(1):
        	self.print_messages()

        self.connect_nodes()

        print("Print how messages propagate through the network. All the nodes should sync (at least blocks should sync, vice-block and round votes may not)")
        for i in range(3):
        	self.print_messages()
        	time.sleep(1)

        # 9th node is now connected
        for n in range(self.num_nodes):
        	assert_equal(self.nodes[n].getblockcount(), blockCount + 2)

if __name__ == '__main__':
    dPoS_P2P().main()
