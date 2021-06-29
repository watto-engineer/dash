Bytz Core version 0.2.0.0
==========================

Release is now available from:

  <https://www.bytz.gg/wallet/>

This is a new major version release, bringing new features, various bugfixes
and other improvements.

This release is mandatory for all nodes and clients.

Please report bugs using the issue tracker at github:

  <https://github.com/bytzcurrency/bytz/issues>


Upgrading and downgrading
=========================

How to Upgrade
--------------

If you are running an older version, shut it down. Wait until it has completely
shut down (which might take a few minutes for older versions), then backup your
wallet and private keys. The new version needs to resync the blockchain. The
easiest way to achieve this, is to rename the old data directory and continue
with a blank data folder. Next, start the new client: run the
installer (on Windows) or just copy over /Applications/Bytz-Qt (on Mac) or
bytzd/bytz-qt (on Linux) and run it. After the client has finished syncing,
shut it down, copy the old wallet.dat to the new wallet subfolder located in the
Bytz data folder, and start the client again.

See the [Bytz wiki page on upgrading](https://github.com/celbalrai/wiki/wiki/How-to-Upgrade-to-Bytz-2.0) for
additional information.

Downgrade warning
-----------------

### Downgrade to a version < 0.2.0.0

Downgrading to a version older than 0.2.0.0 is not supported due to
changes in the database format.

Notable changes
===============

This release bases the Bytz Currency source code on Dash v0.17.x, offering amongst others secure masternode
quorums and additional opcodes for building smarter applications.

The Dash Proof-of-Work protocol has been replaced with the PIVX Proof-of-Stake protocol.

Half of the transaction fees are collected in a Carbon Offset pool in line with Bytz' Carbon Neutral Initiative.

The Dash CoinJoin (formerly PrivateSend) functionality has been disabled. Part of the Bytz Zerocoin functionality has
been ported from the old Bytz codebase to be able to validate old transactions, but no functionality to create
new ZeroCoin transaction is included in the current codebase.

The Atomic Token Protocol (ATP), derived from Andrew Stone/Bitcoin Cash' Token Group proposal, has been extended
with NFT functionality for creating tokens that are in line with ERC-1155 tokens.

Masternode quorum sizes have been reduced in line with the Guardian Validator Nodes specifications. Additionally, gated
access to the GVN network is provided through Guardian Validator Tokens.

### Added and removed RPC commands

All Bytz 0.1 RPC commands have first been replaced by the Dash 0.17 RPC commands. Subsquently, commands have been added
to support token activities and Proof-of-Stake functions, and commands have been removed that related to CoinJoin.
This results in the following changes with respect to the RPC calls present in Dash 0.17:

Added:

|Section|Command|Parameters|
|-|-|-|
|Blockchain|scantxoutset| \<action\> ( \<scanobjects\> )|
|Tokens|configuremanagementtoken|"ticker" "name" decimal_pos "metadata_url" metadata_hash "bls_pubkey" sticky_melt ( confirm_send )|
||configurenft|"name" "mint_amount" "metadata_url" metadata_hash data data_filename ( confirm_send )|
||configuretoken|"ticker" "name" decimal_pos "metadata_url" metadata_hash ( confirm_send )|
||createrawtokentransaction|[{"txid":"id","vout":n},...] {"address":amount,"data":"hex",...} ( locktime )|
||createtokenauthorities|"groupid" "bytzaddress" authoritylist|
||decodetokenmetadata|"data"|
||droptokenauthorities|"groupid" "transactionid" outputnr [ authority1 ( authority2 ... ) ]|
||encodetokenmetadata|{"ticker":"ticker","name":"token name",...}|
||getsubgroupid|"groupid" "data"|
||gettokenbalance|( "groupid" ) ( "address" )|
||gettokentransaction|"txid" ( "blockhash" )|
||listtokenauthorities|( "groupid" )|
||listtokenssinceblock|"groupid" ( "blockhash" target-confirmations includeWatchonly )|
||listtokentransactions|("groupid" count from includeWatchonly )|
||listunspenttokens|( minconf maxconf  ["addresses",...] [include_unsafe] [query_options])|
||melttoken|"groupid" quantity|
||minttoken|"groupid" "bytzaddress" quantity|
||scantokens|\<action\> ( \<tokengroupid\> )|
||sendtoken|"groupid" "address" amount ( "address" amount ) ( .. )|
||signtokenmetadata|"hex_data" "creation_address"|
||tokeninfo|[list, all, stats, groupid, ticker, name] ( "specifier " ) ( "creation_data" ) ( "nft_data" )|
||verifytokenmetadata|"hex_data" "creation_address" "signature"|
|Wallet|autocombinedust |enable ( threshold )|
||getstakingstatus||
||setstakesplitthreshold|value|

Removed:

|Section|Command|Parameters|
|-|-|-|
|Bytz|coinjoin|"command"|
||getcoinjoininfo||
||getpoolinfo||
|Wallet|setcoinjoinamount|amount|
||setcoinjoinrounds|rounds|

Credits
=======

Thanks to everyone who contributed to this release:

- celbalrai
- bytzck
- Matthew
- sackninja

Special thanks to everyone that contributed to the Dash Core development and the
ATP development.
As well as everyone that contributed to testing, submitted issues and reviewed pull requests.

Older releases
==============

Bytz was previously known as Slate.

Bytz Core tree 0.2.x is a fork of Dash Core tree 0.17.x,
Darkcoin was rebranded to Bytz.

These release are considered obsolete. Old release notes can be found here:

- [v0.1.0.0](https://github.com/bytzcurrency/bytz/blob/master/doc/release-notes/bytz/release-notes-0.1.0.md) released June/18/2020
