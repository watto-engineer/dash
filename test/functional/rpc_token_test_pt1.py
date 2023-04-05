#!/usr/bin/env python3
# Copyright (c) 2018-2021 The Wagerr Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test the functionality of all CLI commands.

"""
from test_framework.test_framework import WagerrTestFramework

from test_framework.util import *

from time import sleep
from decimal import Decimal

import re
import sys
import os
import subprocess

WAGERR_TX_FEE = 0.001
WAGERR_AUTH_ADDR = "TDn9ZfHrYvRXyXC6KxRgN6ZRXgJH2JKZWe"

class TokenTest (WagerrTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 2
        self.mn_count = 0
        self.extra_args = [["-debug"], ["-debug"]]
        self.fast_dip3_enforcement = False

    def run_test(self):
        connect_nodes(self.nodes[0], 1)
        tmpdir=self.options.tmpdir
        self.nodes[0].generate(400)
        self.log.info("Generating Tokens...")
        self.nodes[0].importprivkey("TCH8Qby7krfugb2sFWzHQSEmTxBgzBSLkgPtt5EUnzDqfaX9dcsS")
        self.nodes[0].sendtoaddress(WAGERR_AUTH_ADDR, 10)
        self.nodes[0].generate(87)
        self.sync_all()
        self.nodes[0].generate(100)
        self.sync_all()
        self.MGTBLS=self.nodes[0].bls("generate")
        self.ORATBLS=self.nodes[0].bls("generate")
        self.XWAGERRBLS=self.nodes[0].bls("generate")
        self.PARTBLS=self.nodes[0].bls("generate")
        self.LiveBLS=self.nodes[0].bls("generate")
        self.log.info("MGTBLS %s" % self.MGTBLS["public"])
        self.log.info("ORATBLS %s" % self.ORATBLS["public"])
        MGTAddr=self.nodes[0].getnewaddress()
        ORATAddr=self.nodes[0].getnewaddress()
        XWAGERRAddr=self.nodes[0].getnewaddress()
        PARTAddr=self.nodes[0].getnewaddress()
        LIVEAddr=self.nodes[0].getnewaddress()
        HulkAddr=self.nodes[0].getnewaddress()
        self.nodes[0].sendtoaddress(WAGERR_AUTH_ADDR, 10)
        self.nodes[0].generate(1)
        self.MGT=self.nodes[0].configuremanagementtoken( "MGT", "Management", "4", "https://www.google.com", "0",  self.MGTBLS["public"], "false", "true")
        self.log.info("MGT %s" % self.MGT)
        MGTGroup_ID=self.MGT['groupID']
        self.nodes[0].generate(1)
        self.nodes[0].minttoken(MGTGroup_ID, MGTAddr, '82')
        self.nodes[0].sendtoaddress(WAGERR_AUTH_ADDR, 10)
        self.nodes[0].generate(1)
        self.ORAT=self.nodes[0].configuremanagementtoken( "ORAT", "ORAT", "4", "https://www.google.com", "0",  self.ORATBLS["public"], "false", "true")
        self.nodes[0].generate(1)
        self.log.info("ORAT %s" % self.ORAT)
        ORATGroup_ID=self.ORAT['groupID']
        self.nodes[0].minttoken(ORATGroup_ID, ORATAddr, '82')
        self.nodes[0].sendtoaddress(WAGERR_AUTH_ADDR, 10)
        self.nodes[0].generate(1)
        self.sync_all()
        XWAGERRTok=self.nodes[0].configuremanagementtoken("XWAGERR", "ExtraWagerr", "0", "https://github.com/wagerr/ATP-descriptions/blob/master/WAGERR-testnet-XWAGERR.json","f5125a90bde180ef073ce1109376d977f5cbddb5582643c81424cc6cc842babd", self.XWAGERRBLS["public"], "true", "true")
        XWAGERRGroup_ID=XWAGERRTok['groupID']
        self.nodes[0].sendtoaddress(WAGERR_AUTH_ADDR, 10)
        PARTTok=self.nodes[0].configuremanagementtoken("PART", "PartWagerr","0", "https://github.com/wagerr/ATP-descriptions/blob/master/WAGERR-testnet-PART.json", "b0425ee4ba234099970c53c28288da749e2a1afc0f49856f4cab82b37f72f6a5", self.PARTBLS["public"], "true", "true")
        PARTGroup_ID=PARTTok['groupID']
        LIVETok=self.nodes[0].configuremanagementtoken("LIVE", "LiveWagerr","13", "https://github.com/wagerr/ATP-descriptions/blob/master/WAGERR-testnet-LIVE.json", "6de2409add060ec4ef03d61c0966dc46508ed3498e202e9459e492a372ddccf5", self.LiveBLS["public"], "true", "true")
        LIVEGroup_ID=LIVETok['groupID']
        self.nodes[0].sendtoaddress(WAGERR_AUTH_ADDR, 10)
        self.nodes[0].generate(1)
        self.log.info("Token Info %s" % json.dumps(self.nodes[0].tokeninfo("all"), indent=4))
        self.nodes[0].minttoken(MGTGroup_ID, MGTAddr, '4975')
        self.nodes[0].generate(1)
        self.nodes[0].minttoken(XWAGERRGroup_ID, XWAGERRAddr, '71')
        self.nodes[0].generate(1)
        self.nodes[0].minttoken(PARTGroup_ID, PARTAddr, '100')
        self.nodes[0].generate(1)
        self.nodes[0].minttoken(LIVEGroup_ID, LIVEAddr, '1')
        self.nodes[0].generate(1)
        HULKTok=self.nodes[0].configuretoken("HULK", "HulkToken", "10", "https://raw.githubusercontent.com/CeForce/hulktoken/master/hulk.json", "367750e31cb276f5218c013473449c9e6a4019fed603d045b51e25f5db29283a", "true")
        HulkGroup_ID=HULKTok['groupID']
        self.nodes[0].generate(1)
        self.nodes[0].minttoken(HulkGroup_ID, HulkAddr, '15')
        self.nodes[0].generate(1) 
        tokenBalance=self.nodes[0].gettokenbalance()
        for balance in tokenBalance:
            self.log.info("Token Name %s" % balance['name'])
            self.log.info("Token Balance %s" % balance['balance'])
        self.log.info("Hulk Ticker %s" % json.dumps(self.nodes[0].tokeninfo('ticker', 'Hulk'), indent=4))
        self.log.info("Hulk Scan Tokens %s" % self.nodes[0].scantokens('start', HulkGroup_ID))
        tokenAuth=self.nodes[0].listtokenauthorities()
        for authority in tokenAuth:
            self.log.info("Ticker %s" % authority['ticker'])
            self.log.info("Authority address %s\n" % authority['address'])
            self.log.info("Token Authorities %s" % authority['tokenAuthorities'])
        self.log.info("Drop Mint Authoritiy for Hulk")
        HulkGroup=self.nodes[0].listtokenauthorities(HulkGroup_ID)
        self.nodes[0].droptokenauthorities(HulkGroup_ID, HulkGroup[0]['txid'], str(HulkGroup[0]['vout']), 'mint')
        self.nodes[0].generate(1)
        tokenAuthority=(self.nodes[0].listtokenauthorities(HulkGroup_ID))
        tokenHulkAddr=tokenAuthority[0]['address']
        self.log.info("Token authorities Hulk %s\n" % tokenAuthority)
        try:
            self.log.info("Try minting Hulk tokens with mint flag removed")
            self.nodes[0].minttoken(HulkGroup_ID, HulkAddr, '100')
        except JSONRPCException as e:
            print(e)
            assert(e.error['code']==-6)
        self.log.info("Try to Re-Enable mint Hulk")
        try:
            self.nodes[0].createtokenauthorities(HulkGroup_ID, tokenHulkAddr, 'mint')
            self.nodes[0].generate(1)
        except JSONRPCException as e:
            print(e)
            assert(e.error['code']==-32602)
        self.log.info("Mint 100 Hulk tokens")
        try:
            self.log.info("Try minting Hulk tokens with mint flag removed")
            self.nodes[0].minttoken(HulkGroup_ID, HulkAddr, '100')
        except JSONRPCException as e:
            print(e)
            assert(e.error['code']==-6)
        self.log.info("Hulk Scan Tokens %s" % self.nodes[0].scantokens('start', HulkGroup_ID))
        tokenBalance=self.nodes[0].gettokenbalance()
        for balance in tokenBalance:
            self.log.info("Token Name %s" % balance['name'])
            self.log.info("Token Balance %s" % balance['balance'])
        PARTBalance=self.nodes[0].gettokenbalance(PARTGroup_ID)
        self.log.info("PARTWagerr Balance %s"  % PARTBalance['balance'])
        self.log.info("Melt 10 tokens from PartWagerr Group")
        self.nodes[0].melttoken(PARTGroup_ID, '10')
        PARTBalance=self.nodes[0].gettokenbalance(PARTGroup_ID)
        self.log.info("PART Balance %s\n"  % PARTBalance['balance'])
        self.log.info("Token info all (from node1)\n%s\n" % json.dumps(self.nodes[1].tokeninfo('all'), indent=4))
        self.log.info("Token info ticker Hulk\n%s\n" % json.dumps(self.nodes[0].tokeninfo('ticker', 'Hulk'), indent=4))
        self.log.info("Token info name ExtraWagerr\n%s\n" % json.dumps(self.nodes[0].tokeninfo('name', 'ExtraWagerr'), indent=4))
        self.log.info("Token info groupid %s\n%s\n" % (HulkGroup_ID, json.dumps(self.nodes[0].tokeninfo('groupid', HulkGroup_ID), indent=4)))
        LIVE_Trans=self.nodes[0].listtokentransactions(LIVEGroup_ID)
        self.log.info("Token Transactions LiveWagerr Token\n%s\n" % LIVE_Trans)
        LIVETransTXID=LIVE_Trans[0]['txid']
        self.nodes[0].generate(1)
        LIVE_FullTrans=self.nodes[0].gettokentransaction(LIVETransTXID)
        self.log.info("LiveWagerr Transaction\n%s" % self.nodes[0].gettokentransaction(LIVETransTXID))
        LIVE_BlockCount=LIVE_FullTrans['height']
        LIVE_BlockHash=self.nodes[0].getblockhash(LIVE_BlockCount)
        self.log.info("Blockhash block %s %s", LIVE_BlockCount, LIVE_BlockHash)
        self.log.info("\nTransaction ID %s" % LIVETransTXID)
        self.log.info("Transaction Details %s" % self.nodes[0].gettokentransaction(LIVETransTXID, LIVE_BlockHash))
        self.log.info("\nList tokens since block 200 ORAT\n%s" % self.nodes[0].listtokenssinceblock(LIVEGroup_ID, LIVE_BlockHash))
        tokenORATUnspent=self.nodes[0].listunspenttokens(ORATGroup_ID)
        newORAT=self.nodes[0].getnewaddress()
        self.log.info("Send tokens to new address %s" % self.nodes[0].sendtoken(ORATGroup_ID, newORAT, 2))
        self.nodes[0].generate(1)
        self.log.info(self.nodes[1].getaddressbalance)
        subgroupID=self.nodes[0].getsubgroupid(ORATGroup_ID,"credit")
        self.log.info("Subgroup Info %s " % self.nodes[0].tokeninfo('groupid',subgroupID))
        self.log.info("\nUnspent Tokens ORAT Token\n%s\n" % tokenORATUnspent)
        tokenReceiveAddr=self.nodes[1].getnewaddress()
        rawTxid=tokenORATUnspent[0]['txid']
        rawVout=tokenORATUnspent[0]['vout']
        rawAddr=tokenReceiveAddr
        rawAmount=0.01
        self.log.info("txid %s" % rawTxid)
        self.log.info("vout %s" % rawVout)
        self.log.info("recaddr %s" % rawAddr)
        self.log.info("amount %s" % rawAmount )
        inputs=[{ "txid" : rawTxid, "vout" : rawVout }]
        inputs = []
        outputs={ rawAddr : rawAmount }
        token={ "groupid" : ORATGroup_ID, "token_amount" : 0.1 }
        self.log.info(str(inputs))
        self.log.info(outputs)
        self.log.info(token)
        # ICC 86
        #rawtx=self.nodes[0].createrawtokentransaction(inputs, outputs, token)
        #self.log.info(rawtx)
if __name__ == '__main__':
    TokenTest().main()