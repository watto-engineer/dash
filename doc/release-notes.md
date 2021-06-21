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
shut down (which might take a few minutes for older versions), then run the
installer (on Windows) or just copy over /Applications/Bytz-Qt (on Mac) or
bytzd/bytz-qt (on Linux). If you upgrade after DIP0003 activation and you were
using version < 0.13 you will have to reindex (start with -reindex-chainstate
or -reindex) to make sure your wallet has all the new data synced. Upgrading
from version 0.13 should not require any additional actions.

When upgrading from a version prior to 0.14.0.3, the
first startup of Bytz Core will run a migration process which can take a few
minutes to finish. After the migration, a downgrade to an older version is only
possible with a reindex (or reindex-chainstate).

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

Credits
=======

Thanks to everyone who directly contributed to this release:

- celbalrai
- bytzck
- Matthew

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
