#!/usr/bin/env python3
# Copyright (c) 2018-2021 The Bytz Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test the functionality of all CLI commands.

"""
from test_framework.test_framework import BitcoinTestFramework

from test_framework.util import *

from time import sleep
from decimal import Decimal

import re
import sys
import os
import subprocess

BYTZ_TX_FEE = 0.001
BYTZ_AUTH_ADDR = "TqMgq4qkw7bGxf6CDhtDfEqzEtWD5C7x8U"

class TokenTest (BitcoinTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 2
        #self.extra_args = [["-debug"],["-debug"]]

    def run_test(self):
        connect_nodes_bi(self.nodes, 0, 1)
        tmpdir=self.options.tmpdir
        self.log.info("Generating Tokens...")
        self.nodes[0].generate(100)
        self.nodes[0].importprivkey("5rE5LTDq3tRhaPW3RT1De35MocGc9wD8foaBGioxSXJsn45XaFG")
        self.nodes[0].generate(100)
        self.nodes[0].generate(100)
        self.nodes[0].sendtoaddress(BYTZ_AUTH_ADDR, 10)
        self.nodes[0].generate(1)
        MGTBLS=self.nodes[0].bls("generate")
        GVTBLS=self.nodes[0].bls("generate")
        XBYTZBLS=self.nodes[0].bls("generate")
        PARTBLS=self.nodes[0].bls("generate")
        LiveBLS=self.nodes[0].bls("generate")
        HulkBLS=self.nodes[0].bls("generate")
        self.log.info("MGTBLS %s" % MGTBLS["public"])
        MGTAddr=self.nodes[0].getnewaddress()
        GVTAddr=self.nodes[0].getnewaddress()
        XBYTZAddr=self.nodes[0].getnewaddress()
        PARTAddr=self.nodes[0].getnewaddress()
        LIVEAddr=self.nodes[0].getnewaddress()
        HulkAddr=self.nodes[0].getnewaddress()
        MGT=self.nodes[0].configuremanagementtoken( "MGT", "Management", "https://www.google.com", "0", "4", MGTBLS["public"], "false", "true")
        self.nodes[0].generate(1)
        self.log.info("MGT %s" % MGT)
        MGTGroup_ID=MGT['groupID']
        self.nodes[0].minttoken(MGTGroup_ID, MGTAddr, '82')
        GVT=self.nodes[0].configuremanagementtoken("GVT", "GuardianValidator", "https://www.google.com", "0", "0", GVTBLS["public"], "false", "true")
        self.nodes[0].generate(1)
        self.log.info("GVT %s" % GVT)
        GVTGroup_ID=GVT['groupID']
        self.nodes[0].minttoken(GVTGroup_ID, GVTAddr, '43')
        mintaddr=self.nodes[0].getnewaddress()
        self.nodes[0].minttoken(MGTGroup_ID, mintaddr, 500)
        self.nodes[0].generate(1)
        XBYTZTok=self.nodes[0].configuremanagementtoken("XBYTZ", "ExtraBytz", "https://github.com/bytzcurrency/ATP-descriptions/blob/master/BYTZ-testnet-XBYTZ.json","f5125a90bde180ef073ce1109376d977f5cbddb5582643c81424cc6cc842babd","0", XBYTZBLS["public"], "true", "true")
        XBYTZGroup_ID=XBYTZTok['groupID']
        PARTTok=self.nodes[0].configuremanagementtoken("PART", "PartBytz", "https://github.com/bytzcurrency/ATP-descriptions/blob/master/BYTZ-testnet-PART.json", "b0425ee4ba234099970c53c28288da749e2a1afc0f49856f4cab82b37f72f6a5","0", PARTBLS["public"], "true", "true")
        PARTGroup_ID=PARTTok['groupID']
        LIVETok=self.nodes[0].configuremanagementtoken("LIVE", "LiveBytz", "https://github.com/bytzcurrency/ATP-descriptions/blob/master/BYTZ-testnet-LIVE.json", "6de2409add060ec4ef03d61c0966dc46508ed3498e202e9459e492a372ddccf5", "13", LiveBLS["public"], "true", "true")
        LIVEGroup_ID=LIVETok['groupID']
        self.nodes[0].generate(1)
        self.log.info("Token Info %s" % json.dumps(self.nodes[0].tokeninfo("all"), indent=4))
        self.nodes[0].minttoken(MGTGroup_ID, MGTAddr, '4975')
        self.nodes[0].generate(1)
        self.nodes[0].minttoken(XBYTZGroup_ID, XBYTZAddr, '71')
        self.nodes[0].generate(1)
        self.nodes[0].minttoken(PARTGroup_ID, PARTAddr, '100')
        self.nodes[0].generate(1)
        self.nodes[0].minttoken(LIVEGroup_ID, LIVEAddr, '1')
        self.nodes[0].generate(1)
        HULKTok=self.nodes[0].configuretoken("HULK", "HulkToken", "https://raw.githubusercontent.com/CeForce/hulktoken/master/hulk.json", "367750e31cb276f5218c013473449c9e6a4019fed603d045b51e25f5db29283a", "10", "true")
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
        self.log.info("PARTBytz Balance %s"  % PARTBalance['balance'])
        self.log.info("Melt 10 tokens from PartBytz Group")
        self.nodes[0].melttoken(PARTGroup_ID, '10')
        PARTBalance=self.nodes[0].gettokenbalance(PARTGroup_ID)
        self.log.info("PART Balance %s\n"  % PARTBalance['balance'])
        self.log.info("Token info all (from node1)\n%s\n" % json.dumps(self.nodes[1].tokeninfo('all'), indent=4))
        self.log.info("Token info ticker Hulk\n%s\n" % json.dumps(self.nodes[0].tokeninfo('ticker', 'Hulk'), indent=4))
        self.log.info("Token info name ExtraBytz\n%s\n" % json.dumps(self.nodes[0].tokeninfo('name', 'ExtraBytz'), indent=4))
        self.log.info("Token info groupid %s\n%s\n" % (HulkGroup_ID, json.dumps(self.nodes[0].tokeninfo('groupid', HulkGroup_ID), indent=4)))
        LIVE_Trans=self.nodes[0].listtokentransactions(LIVEGroup_ID)
        self.log.info("Token Transactions LiveBytz Token\n%s\n" % LIVE_Trans)
        LIVETrans=LIVE_Trans[0]['txid']
        LIVE_BlockHash=self.nodes[0].getblockhash(200)
        self.log.info("LiveBytz Transaction\n%s" % self.nodes[0].gettokentransaction(LIVETrans))
        self.log.info("Blockhash block 200 %s" % LIVE_BlockHash)
        self.log.info("\nTransaction ID %s" % LIVETrans)
        self.log.info("Transaction Details %s" % self.nodes[0].gettokentransaction(LIVETrans, LIVE_BlockHash))
        self.log.info("\nList tokens since block 200 GVT\n%s" % self.nodes[0].listtokenssinceblock(LIVEGroup_ID, LIVE_BlockHash))
        tokenGVTUnspent=self.nodes[0].listunspenttokens(GVTGroup_ID)
        newGVT=self.nodes[0].getnewaddress()
        self.log.info("Send tokens to new address %s" % self.nodes[0].sendtoken(GVTGroup_ID, newGVT, 2))
        self.nodes[0].generate(1)
        self.log.info(self.nodes[1].getaddressbalance)
        subgroupID=self.nodes[0].getsubgroupid(GVTGroup_ID,"credit")
        self.log.info("Subgroup Info %s " % self.nodes[0].tokeninfo('groupid',subgroupID))
        self.log.info("\nUnspent Tokens GVT Token\n%s\n" % tokenGVTUnspent)
        tokenReceiveAddr=self.nodes[1].getnewaddress()
        rawTxid=tokenGVTUnspent[0]['txid']
        rawVout=tokenGVTUnspent[0]['vout']
        rawAddr=tokenReceiveAddr
        rawAmount=0.01
        self.log.info("txid %s" % rawTxid)
        self.log.info("vout %s" % rawVout)
        self.log.info("recaddr %s" % rawAddr)
        self.log.info("amount %s" % rawAmount )
        inputs=[{ "txid" : rawTxid, "vout" : rawVout }]
        inputs = []
        outputs={ rawAddr : rawAmount }
        token={ "groupid" : GVTGroup_ID, "token_amount" : 0.1 }
        self.log.info(str(inputs))
        self.log.info(outputs)
        self.log.info(token)
        # ICC 86
        #rawtx=self.nodes[0].createrawtokentransaction(inputs, outputs, token)
        #self.log.info(rawtx)
if __name__ == '__main__':
    TokenTest().main()