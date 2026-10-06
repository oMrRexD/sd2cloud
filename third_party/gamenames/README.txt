gamename.csv is the list of PS2 games (ID;name) of HDL Batch Installer, by Matias Israelson (El_isra):
https://github.com/israpps/HDL-Batch-installer, Database/gamename.csv, as of commit
0fb8ba0f8cb3c15caf8f63df80373edd198a3b68. It is distributed under the GNU General Public License, version 3, the
license SD2Cloud is under too (see LICENSE at the root of this repository).

The sd2psXtd firmware builds its own list of game names from the same file. tools/make_gamenames.py turns it into
assets/gamenames.txt, which is embedded in SD2Cloud.
