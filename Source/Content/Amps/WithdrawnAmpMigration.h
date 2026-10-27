#pragma once
#include <juce_core/juce_core.h>
namespace namamp
{
// Identity-only compatibility data. No withdrawn model weights are distributed.
// These five captures were withdrawn by the publisher on 15 October 2026.
inline bool withdrawnSilverJubilee(const juce::String& hash)
{
    static const char* hashes[]{"82fa446758f20d7994dec3df6578d31a1c4aafc0743727a2684480db646cde85","964672fd9b2ab0bb7aed8a00a69fdfa855e0885623ded9486629b34558fdfdb0","7aa1084d47124551a24e3596db848fe21355e86829d75c8673ef190412f1b22b","dfbdf7f0fedffe6cd7a6486cacc17bef1e1ab5244b8217660726969ad7b0285c","6dd5d6e9a91a5a623878e7d8d47828dbc5332c19e7dc2e7b4628b931a700b6b9"};
    for(const auto* retired:hashes)if(hash==retired)return true;
    return false;
}

template<class Identity> inline bool migrateWithdrawnAmp(Identity& value)
{
    struct Replacement{const char* oldHash;const char* file;const char* sha256;juce::int64 bytes;};
    static const Replacement replacements[]{
{"82fa446758f20d7994dec3df6578d31a1c4aafc0743727a2684480db646cde85","05 - High Gain/Wizard Modern Classic MCII 100W/Wzd.MCII100-Mes412-LD-BST-HI Bandit Torn - BLEND #1.nam","3bb5c6ecfcf59a8c600db95df7da7c7b1e2ae447f7ee30904cffbcda85702707",294515},
{"964672fd9b2ab0bb7aed8a00a69fdfa855e0885623ded9486629b34558fdfdb0","05 - High Gain/Wizard Modern Classic MCII 100W/Wzd.MCII100-Mes412-LD-BST-HI Bandit Torn - BLEND #1.nam","3bb5c6ecfcf59a8c600db95df7da7c7b1e2ae447f7ee30904cffbcda85702707",294515},
{"7aa1084d47124551a24e3596db848fe21355e86829d75c8673ef190412f1b22b","05 - High Gain/Wizard Modern Classic MCII 100W/Wzd.MCII100-Mes412-LD-BST-HI Bandit Torn - BLEND #1.nam","3bb5c6ecfcf59a8c600db95df7da7c7b1e2ae447f7ee30904cffbcda85702707",294515},
{"dfbdf7f0fedffe6cd7a6486cacc17bef1e1ab5244b8217660726969ad7b0285c","05 - High Gain/Wizard Modern Classic MCII 100W/Wzd.MCII100-Mes412-LD-BST-HI Bandit Torn - BLEND #1.nam","3bb5c6ecfcf59a8c600db95df7da7c7b1e2ae447f7ee30904cffbcda85702707",294515},
{"6dd5d6e9a91a5a623878e7d8d47828dbc5332c19e7dc2e7b4628b931a700b6b9","05 - High Gain/Wizard Modern Classic MCII 100W/Wzd.MCII100-Mes412-LD-BST-HI Bandit Torn - BLEND #1.nam","3bb5c6ecfcf59a8c600db95df7da7c7b1e2ae447f7ee30904cffbcda85702707",294515},

{"3339ac765f7b75ba1dd776108f51b85bf8cff1bba0f0e66fbab688d5e83cc8d6","01 - Clean/Fender Ramparte Pawnshop/Fender Pawn Shop - Ramparte -C3 - by ClayBennett.nam","53fe08f06a5fd9bae43c67a03554898eba42a31c57838cf41ab8f736775ba3c5",296667},
{"12b17a5b8a81a6977f03765632be6d0f3a3fb7001dc48c76f6abeb7790cfb6f4","01 - Clean/Fender Ramparte Pawnshop/Fender Pawn Shop - Ramparte -C3 - by ClayBennett.nam","53fe08f06a5fd9bae43c67a03554898eba42a31c57838cf41ab8f736775ba3c5",296667},
{"58ac5604d5b6c62d5e9720db90e07dae80262c1ac4a01aef65f78e45b716f68c","01 - Clean/Fender Ramparte Pawnshop/Fender Pawn Shop - Ramparte -C3 - by ClayBennett.nam","53fe08f06a5fd9bae43c67a03554898eba42a31c57838cf41ab8f736775ba3c5",296667},
{"b5747ab6c1cdf8e7bc56c3d54761227d58360c4fa1297c79da60418529740826","01 - Clean/Fender Ramparte Pawnshop/Fender Pawn Shop - Ramparte -C3 - by ClayBennett.nam","53fe08f06a5fd9bae43c67a03554898eba42a31c57838cf41ab8f736775ba3c5",296667},
{"f9021a3b60ac1fb16a1c88449d39beb4ed9f9f70ed10c2f8dccfb2b09b63db61","01 - Clean/Fender Ramparte Pawnshop/Fender Pawn Shop - Ramparte -C3 - by ClayBennett.nam","53fe08f06a5fd9bae43c67a03554898eba42a31c57838cf41ab8f736775ba3c5",296667},
{"0eb0edc55eb03777811316f465f2a7a0bd0a196a1dd9b00d31d06f982d2633d8","01 - Clean/Fender Ramparte Pawnshop/Fender Pawn Shop - Ramparte -C3 - by ClayBennett.nam","53fe08f06a5fd9bae43c67a03554898eba42a31c57838cf41ab8f736775ba3c5",296667},
{"300f5a304dc997bdc7e983ce91966ff28928f13349ea119e05267ca6d2de34d5","01 - Clean/Fender Ramparte Pawnshop/Fender Pawn Shop - Ramparte -C3 - by ClayBennett.nam","53fe08f06a5fd9bae43c67a03554898eba42a31c57838cf41ab8f736775ba3c5",296667},
{"f3a9dfffeea73732bffc27d3eafb44a19f59e968e4c5215d61e9fb6646418c6c","01 - Clean/Fender Ramparte Pawnshop/Fender Pawn Shop - Ramparte -C3 - by ClayBennett.nam","53fe08f06a5fd9bae43c67a03554898eba42a31c57838cf41ab8f736775ba3c5",296667},
{"823a96ef8281e48d5a6858a2e4cfe41118170d20b67e7df0dbef7d53faf47afb","02 - Edge/Fender Ramparte Pawnshop/Fender Pawn Shop - Ramparte - C5 - by ClayBennett.nam","0114b4ee79d48b91de4b6c1c8e7c91f5b3c1503aec6db92f3a64e66733eda802",295773},
{"65fe463e6d1dfa9dbee31267bc9de783062f1e9d3f31af7f9f120f67f8d922dc","02 - Edge/Fender Ramparte Pawnshop/Fender Pawn Shop - Ramparte - C5 - by ClayBennett.nam","0114b4ee79d48b91de4b6c1c8e7c91f5b3c1503aec6db92f3a64e66733eda802",295773},
{"9096a62fff5692ccdffcd784276b7b97fd2b89c8e45868fc3d9bc5851bd04893","02 - Edge/Fender Ramparte Pawnshop/Fender Pawn Shop - Ramparte - C5 121 - by ClayBennett.nam","abcfcbbf3fb5016c37d05248cedaba1c2d2418444c47b4949de71dc1ffb460f8",296046},
{"0f19cc76f68ad144491c6bc25f23a5a45f75209cbe238b9c1a0c2adab4e9cc6b","02 - Edge/Fender Ramparte Pawnshop/Fender Pawn Shop - Ramparte - C5 121 - by ClayBennett.nam","abcfcbbf3fb5016c37d05248cedaba1c2d2418444c47b4949de71dc1ffb460f8",296046},
{"a25fd162716511bba173b22caa3673b243d27b61af57ad201d685b75b72fcfbc","02 - Edge/Fender Ramparte Pawnshop/Fender Pawn Shop - Ramparte - C5 121 - by ClayBennett.nam","abcfcbbf3fb5016c37d05248cedaba1c2d2418444c47b4949de71dc1ffb460f8",296046},
{"6e7fdd70dee1ca9283376176dd07b4d7418e6c38e9f25f73e2a4e1e2b933fbc3","02 - Edge/Fender Ramparte Pawnshop/Fender Pawn Shop - Ramparte - C5 121 - by ClayBennett.nam","abcfcbbf3fb5016c37d05248cedaba1c2d2418444c47b4949de71dc1ffb460f8",296046},
{"c3b88030eefe539295b270260c7a2d6b05402a01a2626050c05bdf2865a8d6fa","02 - Edge/Fender Ramparte Pawnshop/Fender Pawn Shop - Ramparte - C5 - by ClayBennett.nam","0114b4ee79d48b91de4b6c1c8e7c91f5b3c1503aec6db92f3a64e66733eda802",295773},
{"0639e82ab68affc8c73436e1368493275e22d477b2f3f0a712aa41557918a2e6","02 - Edge/Fender Ramparte Pawnshop/Fender Pawn Shop - Ramparte - C5 - by ClayBennett.nam","0114b4ee79d48b91de4b6c1c8e7c91f5b3c1503aec6db92f3a64e66733eda802",295773},
{"eb0382b8d6cdec8e4c9a9e7bcc0e1b730c931e0d6ab3c55659f154b938de0ba5","02 - Edge/Fender Ramparte Pawnshop/Fender Pawn Shop - Ramparte - C5 121 - by ClayBennett.nam","abcfcbbf3fb5016c37d05248cedaba1c2d2418444c47b4949de71dc1ffb460f8",296046},
{"c652ed72357c5d5a379693f21817c561b6f19e887ab145c2dc90519f55e3f655","02 - Edge/Fender Ramparte Pawnshop/Fender Pawn Shop - Ramparte - C5 121 - by ClayBennett.nam","abcfcbbf3fb5016c37d05248cedaba1c2d2418444c47b4949de71dc1ffb460f8",296046},
{"f19e9cd7e167cbca4ab32f20ad267f709ee4c811dcae188a825b75641317d8a6","02 - Edge/Fender Ramparte Pawnshop/Fender Pawn Shop - Ramparte - C5 121 - by ClayBennett.nam","abcfcbbf3fb5016c37d05248cedaba1c2d2418444c47b4949de71dc1ffb460f8",296046},
{"30538ddeab3790bc882a9e61c5a118eeb497afbbf9482636af48241603d9c34d","02 - Edge/Fender Ramparte Pawnshop/Fender Pawn Shop - Ramparte - C5 121 - by ClayBennett.nam","abcfcbbf3fb5016c37d05248cedaba1c2d2418444c47b4949de71dc1ffb460f8",296046},
    };
    for(const auto& item:replacements)if(value.sha256==item.oldHash){value={item.file,item.sha256,item.bytes};return true;}
    return false;
}
}
