BYTZ Core version *0.1.0* is now available from:

  <https://github.com/bytzcurrency/bytz/releases/tag/v0.1.0>

This is the first major version release, including new features, various
bugfixes and performance improvements, as well as updated translations.

Please report bugs using the issue tracker at GitHub:

  <https://github.com/bytzcurrency/bytz/issues>

How to Upgrade
==============

If you are running an older version, shut it down. Wait until it has completely
shut down (which might take a few minutes for older versions), then run the
installer (on Windows) or just copy over `/Applications/BYTZ-Qt` (on Mac)
or `bytzd`/`bytz-qt` (on Linux).

Compatibility
==============

BYTZ Core is extensively tested on multiple operating systems using
the Linux kernel, macOS 10.8+, and Windows Vista and later. Windows XP is not supported.

BYTZ Core should also work on most other Unix-like systems but is not
frequently tested on them.

Notable changes
===============

Do not allow generation of stakes with not enough coins
-------------------------------------------------------

Protections were in place to guard against stakes with more coins than the
expected amount, update this to also guard against stakes with less coins than
the expected amounts.

0.1.0 Change log
=================

- `18aba2b` Artworksedit, cleanup, update sources (#4) (Bytz Random Dev)
- `b4d918d` release 0.0.99, PoW miner (Bytz Random Dev)
- `6a9b586` Do not allow generation of stakes with not enough coins (blockcig)
- `35b69f9` Add checkpoint on block 67000 (Bytz Random Dev)
- `01068bc` Version and protocol upgrade (Bytz Random Dev)
- `016316b` release branch 0.1.00 (Bytz Random Dev)
- `3b131bb` Prepare release 0.1.00 (Bytz Random Dev)

Credits
=======

Thanks to everyone who directly (or indirectly through cherry-picks) contributed
to this release:

- blockcig
- Cory Fields
- Fuzzbawls
- Gediminas
- gpdionisio
- Mrs-X
- presstab
- SHTDJ
- Bytz Random Dev

As well as everyone that helped translating on Transifex.
