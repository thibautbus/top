/* Force-included into Lua's lstate.c alone (CMakeLists): a fixed string hash
 * seed in place of the one lstate.c makes from the clock and from addresses,
 * so that a mod's tables are laid out the same in every run.  Lua's sources
 * stay unmodified (third_party/lua/VERSION.md). */
#define luai_makeseed(L) 0x4f524143u
