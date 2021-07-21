Bytz Core staging tree 0.2
===========================

Current Build State

[![bytzcurrency](https://circleci.com/gh/bytzcurrency/BYTZ.svg?style=shield)](https://app.circleci.com/pipelines/github/bytzcurrency)

Download the latest release from github

[![Latest release](https://img.shields.io/github/release/bytzcurrency/BYTZ.svg)](https://github.com/bytzcurrency/BYTZ/releases/latest) ![Latest stable Release](https://img.shields.io/github/downloads/bytzcurrency/BYTZ/latest/total.svg?style=social)

Install the latest release from the Snap Store

[![Get it from the Snap Store](https://snapcraft.io/static/images/badges/en/snap-store-black.svg)](https://snapcraft.io/bytz)

BYTZ Officlal Web Page

https://www.bytz.gg


What is Bytz?
-------------

BYTZ is a blockchain-based entertainment utility protocol powered by a
cryptographically secure multilayered network. Decentralized delivery
yields low-cost, high-speed, high-definition media access globally.
Consumers will be able to spend BYTZ cryptocurrency (BYTZ) on some
of the best entertainment the world has to offer. Tickets will be forgery
resistant, virtually eliminating fraud. Service providers holding BYTZ can
earn even more by storing and delivering content.

Together with this use case BYTZ also features fast transactions with low
transaction fees and a low environmental footprint. It utilizes a custom Proof
of Stake protocol for securing its network and uses an innovative token system
to bring you network validated, secure fungible and non-fungible tokens.

For more information, as well as an immediately useable, binary version of
the Bytz Core software, see https://bytz.gg/getbytz/.


License
-------

Bytz Core is released under the terms of the MIT license. See [COPYING](COPYING) for more
information or see https://opensource.org/licenses/MIT.

Development Process
-------------------

The `master` branch is meant to be stable. Development is normally done in separate branches.
[Tags](https://github.com/bytzcurrency/bytz/tags) are created to indicate new official,
stable release versions of Bytz Core.

The contribution workflow is described in [CONTRIBUTING.md](CONTRIBUTING.md).

Testing
-------

Testing and code review is the bottleneck for development; we get more pull
requests than we can review and test on short notice. Please be patient and help out by testing
other people's pull requests, and remember this is a security-critical project where any mistake might cost people
lots of money.

### Automated Testing

Developers are strongly encouraged to write [unit tests](src/test/README.md) for new code, and to
submit new unit tests for old code. Unit tests can be compiled and run
(assuming they weren't disabled in configure) with: `make check`. Further details on running
and extending unit tests can be found in [/src/test/README.md](/src/test/README.md).

There are also [regression and integration tests](/test), written
in Python, that are run automatically on the build server.
These tests can be run (if the [test dependencies](/test) are installed) with: `test/functional/test_runner.py`

The CircleCI build system makes sure that every pull request is built for Windows, Linux, and OS X, and that unit/sanity tests are run automatically.

### Manual Quality Assurance (QA) Testing

Changes should be tested by somebody other than the developer who wrote the
code. This is especially important for large or high-risk changes. It is useful
to add a test plan to the pull request description if testing the changes is
not straightforward.

Translations
------------

Changes to translations as well as new translations can be submitted to
[Bytz Core's Transifex page](https://www.transifex.com/projects/p/bytz/).

Translations are periodically pulled from Transifex and merged into the git repository. See the
[translation process](doc/translation_process.md) for details on how this works.

**Important**: We do not accept translation changes as GitHub pull requests because the next
pull from Transifex would automatically overwrite them again.

Translators should also follow the [forum](https://www.bytz.gg/forum/topic/bytz-worldwide-collaboration.88/).
