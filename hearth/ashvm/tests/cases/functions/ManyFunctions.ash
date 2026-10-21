package functions;

/// More than 256 functions: calls and closures past index 255 use the 16-bit OP_CALL_WIDE / OP_MAKE_CLOSURE_WIDE.
interface Fn { int apply(int x); }

class ManyFunctions {
    static int f0(int x) { return x + 0; }
    static int f1(int x) { return x + 1; }
    static int f2(int x) { return x + 2; }
    static int f3(int x) { return x + 3; }
    static int f4(int x) { return x + 4; }
    static int f5(int x) { return x + 5; }
    static int f6(int x) { return x + 6; }
    static int f7(int x) { return x + 7; }
    static int f8(int x) { return x + 8; }
    static int f9(int x) { return x + 9; }
    static int f10(int x) { return x + 10; }
    static int f11(int x) { return x + 11; }
    static int f12(int x) { return x + 12; }
    static int f13(int x) { return x + 13; }
    static int f14(int x) { return x + 14; }
    static int f15(int x) { return x + 15; }
    static int f16(int x) { return x + 16; }
    static int f17(int x) { return x + 17; }
    static int f18(int x) { return x + 18; }
    static int f19(int x) { return x + 19; }
    static int f20(int x) { return x + 20; }
    static int f21(int x) { return x + 21; }
    static int f22(int x) { return x + 22; }
    static int f23(int x) { return x + 23; }
    static int f24(int x) { return x + 24; }
    static int f25(int x) { return x + 25; }
    static int f26(int x) { return x + 26; }
    static int f27(int x) { return x + 27; }
    static int f28(int x) { return x + 28; }
    static int f29(int x) { return x + 29; }
    static int f30(int x) { return x + 30; }
    static int f31(int x) { return x + 31; }
    static int f32(int x) { return x + 32; }
    static int f33(int x) { return x + 33; }
    static int f34(int x) { return x + 34; }
    static int f35(int x) { return x + 35; }
    static int f36(int x) { return x + 36; }
    static int f37(int x) { return x + 37; }
    static int f38(int x) { return x + 38; }
    static int f39(int x) { return x + 39; }
    static int f40(int x) { return x + 40; }
    static int f41(int x) { return x + 41; }
    static int f42(int x) { return x + 42; }
    static int f43(int x) { return x + 43; }
    static int f44(int x) { return x + 44; }
    static int f45(int x) { return x + 45; }
    static int f46(int x) { return x + 46; }
    static int f47(int x) { return x + 47; }
    static int f48(int x) { return x + 48; }
    static int f49(int x) { return x + 49; }
    static int f50(int x) { return x + 50; }
    static int f51(int x) { return x + 51; }
    static int f52(int x) { return x + 52; }
    static int f53(int x) { return x + 53; }
    static int f54(int x) { return x + 54; }
    static int f55(int x) { return x + 55; }
    static int f56(int x) { return x + 56; }
    static int f57(int x) { return x + 57; }
    static int f58(int x) { return x + 58; }
    static int f59(int x) { return x + 59; }
    static int f60(int x) { return x + 60; }
    static int f61(int x) { return x + 61; }
    static int f62(int x) { return x + 62; }
    static int f63(int x) { return x + 63; }
    static int f64(int x) { return x + 64; }
    static int f65(int x) { return x + 65; }
    static int f66(int x) { return x + 66; }
    static int f67(int x) { return x + 67; }
    static int f68(int x) { return x + 68; }
    static int f69(int x) { return x + 69; }
    static int f70(int x) { return x + 70; }
    static int f71(int x) { return x + 71; }
    static int f72(int x) { return x + 72; }
    static int f73(int x) { return x + 73; }
    static int f74(int x) { return x + 74; }
    static int f75(int x) { return x + 75; }
    static int f76(int x) { return x + 76; }
    static int f77(int x) { return x + 77; }
    static int f78(int x) { return x + 78; }
    static int f79(int x) { return x + 79; }
    static int f80(int x) { return x + 80; }
    static int f81(int x) { return x + 81; }
    static int f82(int x) { return x + 82; }
    static int f83(int x) { return x + 83; }
    static int f84(int x) { return x + 84; }
    static int f85(int x) { return x + 85; }
    static int f86(int x) { return x + 86; }
    static int f87(int x) { return x + 87; }
    static int f88(int x) { return x + 88; }
    static int f89(int x) { return x + 89; }
    static int f90(int x) { return x + 90; }
    static int f91(int x) { return x + 91; }
    static int f92(int x) { return x + 92; }
    static int f93(int x) { return x + 93; }
    static int f94(int x) { return x + 94; }
    static int f95(int x) { return x + 95; }
    static int f96(int x) { return x + 96; }
    static int f97(int x) { return x + 97; }
    static int f98(int x) { return x + 98; }
    static int f99(int x) { return x + 99; }
    static int f100(int x) { return x + 100; }
    static int f101(int x) { return x + 101; }
    static int f102(int x) { return x + 102; }
    static int f103(int x) { return x + 103; }
    static int f104(int x) { return x + 104; }
    static int f105(int x) { return x + 105; }
    static int f106(int x) { return x + 106; }
    static int f107(int x) { return x + 107; }
    static int f108(int x) { return x + 108; }
    static int f109(int x) { return x + 109; }
    static int f110(int x) { return x + 110; }
    static int f111(int x) { return x + 111; }
    static int f112(int x) { return x + 112; }
    static int f113(int x) { return x + 113; }
    static int f114(int x) { return x + 114; }
    static int f115(int x) { return x + 115; }
    static int f116(int x) { return x + 116; }
    static int f117(int x) { return x + 117; }
    static int f118(int x) { return x + 118; }
    static int f119(int x) { return x + 119; }
    static int f120(int x) { return x + 120; }
    static int f121(int x) { return x + 121; }
    static int f122(int x) { return x + 122; }
    static int f123(int x) { return x + 123; }
    static int f124(int x) { return x + 124; }
    static int f125(int x) { return x + 125; }
    static int f126(int x) { return x + 126; }
    static int f127(int x) { return x + 127; }
    static int f128(int x) { return x + 128; }
    static int f129(int x) { return x + 129; }
    static int f130(int x) { return x + 130; }
    static int f131(int x) { return x + 131; }
    static int f132(int x) { return x + 132; }
    static int f133(int x) { return x + 133; }
    static int f134(int x) { return x + 134; }
    static int f135(int x) { return x + 135; }
    static int f136(int x) { return x + 136; }
    static int f137(int x) { return x + 137; }
    static int f138(int x) { return x + 138; }
    static int f139(int x) { return x + 139; }
    static int f140(int x) { return x + 140; }
    static int f141(int x) { return x + 141; }
    static int f142(int x) { return x + 142; }
    static int f143(int x) { return x + 143; }
    static int f144(int x) { return x + 144; }
    static int f145(int x) { return x + 145; }
    static int f146(int x) { return x + 146; }
    static int f147(int x) { return x + 147; }
    static int f148(int x) { return x + 148; }
    static int f149(int x) { return x + 149; }
    static int f150(int x) { return x + 150; }
    static int f151(int x) { return x + 151; }
    static int f152(int x) { return x + 152; }
    static int f153(int x) { return x + 153; }
    static int f154(int x) { return x + 154; }
    static int f155(int x) { return x + 155; }
    static int f156(int x) { return x + 156; }
    static int f157(int x) { return x + 157; }
    static int f158(int x) { return x + 158; }
    static int f159(int x) { return x + 159; }
    static int f160(int x) { return x + 160; }
    static int f161(int x) { return x + 161; }
    static int f162(int x) { return x + 162; }
    static int f163(int x) { return x + 163; }
    static int f164(int x) { return x + 164; }
    static int f165(int x) { return x + 165; }
    static int f166(int x) { return x + 166; }
    static int f167(int x) { return x + 167; }
    static int f168(int x) { return x + 168; }
    static int f169(int x) { return x + 169; }
    static int f170(int x) { return x + 170; }
    static int f171(int x) { return x + 171; }
    static int f172(int x) { return x + 172; }
    static int f173(int x) { return x + 173; }
    static int f174(int x) { return x + 174; }
    static int f175(int x) { return x + 175; }
    static int f176(int x) { return x + 176; }
    static int f177(int x) { return x + 177; }
    static int f178(int x) { return x + 178; }
    static int f179(int x) { return x + 179; }
    static int f180(int x) { return x + 180; }
    static int f181(int x) { return x + 181; }
    static int f182(int x) { return x + 182; }
    static int f183(int x) { return x + 183; }
    static int f184(int x) { return x + 184; }
    static int f185(int x) { return x + 185; }
    static int f186(int x) { return x + 186; }
    static int f187(int x) { return x + 187; }
    static int f188(int x) { return x + 188; }
    static int f189(int x) { return x + 189; }
    static int f190(int x) { return x + 190; }
    static int f191(int x) { return x + 191; }
    static int f192(int x) { return x + 192; }
    static int f193(int x) { return x + 193; }
    static int f194(int x) { return x + 194; }
    static int f195(int x) { return x + 195; }
    static int f196(int x) { return x + 196; }
    static int f197(int x) { return x + 197; }
    static int f198(int x) { return x + 198; }
    static int f199(int x) { return x + 199; }
    static int f200(int x) { return x + 200; }
    static int f201(int x) { return x + 201; }
    static int f202(int x) { return x + 202; }
    static int f203(int x) { return x + 203; }
    static int f204(int x) { return x + 204; }
    static int f205(int x) { return x + 205; }
    static int f206(int x) { return x + 206; }
    static int f207(int x) { return x + 207; }
    static int f208(int x) { return x + 208; }
    static int f209(int x) { return x + 209; }
    static int f210(int x) { return x + 210; }
    static int f211(int x) { return x + 211; }
    static int f212(int x) { return x + 212; }
    static int f213(int x) { return x + 213; }
    static int f214(int x) { return x + 214; }
    static int f215(int x) { return x + 215; }
    static int f216(int x) { return x + 216; }
    static int f217(int x) { return x + 217; }
    static int f218(int x) { return x + 218; }
    static int f219(int x) { return x + 219; }
    static int f220(int x) { return x + 220; }
    static int f221(int x) { return x + 221; }
    static int f222(int x) { return x + 222; }
    static int f223(int x) { return x + 223; }
    static int f224(int x) { return x + 224; }
    static int f225(int x) { return x + 225; }
    static int f226(int x) { return x + 226; }
    static int f227(int x) { return x + 227; }
    static int f228(int x) { return x + 228; }
    static int f229(int x) { return x + 229; }
    static int f230(int x) { return x + 230; }
    static int f231(int x) { return x + 231; }
    static int f232(int x) { return x + 232; }
    static int f233(int x) { return x + 233; }
    static int f234(int x) { return x + 234; }
    static int f235(int x) { return x + 235; }
    static int f236(int x) { return x + 236; }
    static int f237(int x) { return x + 237; }
    static int f238(int x) { return x + 238; }
    static int f239(int x) { return x + 239; }
    static int f240(int x) { return x + 240; }
    static int f241(int x) { return x + 241; }
    static int f242(int x) { return x + 242; }
    static int f243(int x) { return x + 243; }
    static int f244(int x) { return x + 244; }
    static int f245(int x) { return x + 245; }
    static int f246(int x) { return x + 246; }
    static int f247(int x) { return x + 247; }
    static int f248(int x) { return x + 248; }
    static int f249(int x) { return x + 249; }
    static int f250(int x) { return x + 250; }
    static int f251(int x) { return x + 251; }
    static int f252(int x) { return x + 252; }
    static int f253(int x) { return x + 253; }
    static int f254(int x) { return x + 254; }
    static int f255(int x) { return x + 255; }
    static int f256(int x) { return x + 256; }
    static int f257(int x) { return x + 257; }
    static int f258(int x) { return x + 258; }
    static int f259(int x) { return x + 259; }
    static int f260(int x) { return x + 260; }
    static int f261(int x) { return x + 261; }
    static int f262(int x) { return x + 262; }
    static int f263(int x) { return x + 263; }
    static int f264(int x) { return x + 264; }
    static int f265(int x) { return x + 265; }
    static int f266(int x) { return x + 266; }
    static int f267(int x) { return x + 267; }
    static int f268(int x) { return x + 268; }
    static int f269(int x) { return x + 269; }
    static int f270(int x) { return x + 270; }
    static int f271(int x) { return x + 271; }
    static int f272(int x) { return x + 272; }
    static int f273(int x) { return x + 273; }
    static int f274(int x) { return x + 274; }
    static int f275(int x) { return x + 275; }
    static int f276(int x) { return x + 276; }
    static int f277(int x) { return x + 277; }
    static int f278(int x) { return x + 278; }
    static int f279(int x) { return x + 279; }
    static int f280(int x) { return x + 280; }
    static int f281(int x) { return x + 281; }
    static int f282(int x) { return x + 282; }
    static int f283(int x) { return x + 283; }
    static int f284(int x) { return x + 284; }
    static int f285(int x) { return x + 285; }
    static int f286(int x) { return x + 286; }
    static int f287(int x) { return x + 287; }
    static int f288(int x) { return x + 288; }
    static int f289(int x) { return x + 289; }
    static int f290(int x) { return x + 290; }
    static int f291(int x) { return x + 291; }
    static int f292(int x) { return x + 292; }
    static int f293(int x) { return x + 293; }
    static int f294(int x) { return x + 294; }
    static int f295(int x) { return x + 295; }
    static int f296(int x) { return x + 296; }
    static int f297(int x) { return x + 297; }
    static int f298(int x) { return x + 298; }
    static int f299(int x) { return x + 299; }

    public static void main(String[] args) {
        int t = 0;
        t = t + f0(1);
        t = t + f1(1);
        t = t + f2(1);
        t = t + f3(1);
        t = t + f4(1);
        t = t + f5(1);
        t = t + f6(1);
        t = t + f7(1);
        t = t + f8(1);
        t = t + f9(1);
        t = t + f10(1);
        t = t + f11(1);
        t = t + f12(1);
        t = t + f13(1);
        t = t + f14(1);
        t = t + f15(1);
        t = t + f16(1);
        t = t + f17(1);
        t = t + f18(1);
        t = t + f19(1);
        t = t + f20(1);
        t = t + f21(1);
        t = t + f22(1);
        t = t + f23(1);
        t = t + f24(1);
        t = t + f25(1);
        t = t + f26(1);
        t = t + f27(1);
        t = t + f28(1);
        t = t + f29(1);
        t = t + f30(1);
        t = t + f31(1);
        t = t + f32(1);
        t = t + f33(1);
        t = t + f34(1);
        t = t + f35(1);
        t = t + f36(1);
        t = t + f37(1);
        t = t + f38(1);
        t = t + f39(1);
        t = t + f40(1);
        t = t + f41(1);
        t = t + f42(1);
        t = t + f43(1);
        t = t + f44(1);
        t = t + f45(1);
        t = t + f46(1);
        t = t + f47(1);
        t = t + f48(1);
        t = t + f49(1);
        t = t + f50(1);
        t = t + f51(1);
        t = t + f52(1);
        t = t + f53(1);
        t = t + f54(1);
        t = t + f55(1);
        t = t + f56(1);
        t = t + f57(1);
        t = t + f58(1);
        t = t + f59(1);
        t = t + f60(1);
        t = t + f61(1);
        t = t + f62(1);
        t = t + f63(1);
        t = t + f64(1);
        t = t + f65(1);
        t = t + f66(1);
        t = t + f67(1);
        t = t + f68(1);
        t = t + f69(1);
        t = t + f70(1);
        t = t + f71(1);
        t = t + f72(1);
        t = t + f73(1);
        t = t + f74(1);
        t = t + f75(1);
        t = t + f76(1);
        t = t + f77(1);
        t = t + f78(1);
        t = t + f79(1);
        t = t + f80(1);
        t = t + f81(1);
        t = t + f82(1);
        t = t + f83(1);
        t = t + f84(1);
        t = t + f85(1);
        t = t + f86(1);
        t = t + f87(1);
        t = t + f88(1);
        t = t + f89(1);
        t = t + f90(1);
        t = t + f91(1);
        t = t + f92(1);
        t = t + f93(1);
        t = t + f94(1);
        t = t + f95(1);
        t = t + f96(1);
        t = t + f97(1);
        t = t + f98(1);
        t = t + f99(1);
        t = t + f100(1);
        t = t + f101(1);
        t = t + f102(1);
        t = t + f103(1);
        t = t + f104(1);
        t = t + f105(1);
        t = t + f106(1);
        t = t + f107(1);
        t = t + f108(1);
        t = t + f109(1);
        t = t + f110(1);
        t = t + f111(1);
        t = t + f112(1);
        t = t + f113(1);
        t = t + f114(1);
        t = t + f115(1);
        t = t + f116(1);
        t = t + f117(1);
        t = t + f118(1);
        t = t + f119(1);
        t = t + f120(1);
        t = t + f121(1);
        t = t + f122(1);
        t = t + f123(1);
        t = t + f124(1);
        t = t + f125(1);
        t = t + f126(1);
        t = t + f127(1);
        t = t + f128(1);
        t = t + f129(1);
        t = t + f130(1);
        t = t + f131(1);
        t = t + f132(1);
        t = t + f133(1);
        t = t + f134(1);
        t = t + f135(1);
        t = t + f136(1);
        t = t + f137(1);
        t = t + f138(1);
        t = t + f139(1);
        t = t + f140(1);
        t = t + f141(1);
        t = t + f142(1);
        t = t + f143(1);
        t = t + f144(1);
        t = t + f145(1);
        t = t + f146(1);
        t = t + f147(1);
        t = t + f148(1);
        t = t + f149(1);
        t = t + f150(1);
        t = t + f151(1);
        t = t + f152(1);
        t = t + f153(1);
        t = t + f154(1);
        t = t + f155(1);
        t = t + f156(1);
        t = t + f157(1);
        t = t + f158(1);
        t = t + f159(1);
        t = t + f160(1);
        t = t + f161(1);
        t = t + f162(1);
        t = t + f163(1);
        t = t + f164(1);
        t = t + f165(1);
        t = t + f166(1);
        t = t + f167(1);
        t = t + f168(1);
        t = t + f169(1);
        t = t + f170(1);
        t = t + f171(1);
        t = t + f172(1);
        t = t + f173(1);
        t = t + f174(1);
        t = t + f175(1);
        t = t + f176(1);
        t = t + f177(1);
        t = t + f178(1);
        t = t + f179(1);
        t = t + f180(1);
        t = t + f181(1);
        t = t + f182(1);
        t = t + f183(1);
        t = t + f184(1);
        t = t + f185(1);
        t = t + f186(1);
        t = t + f187(1);
        t = t + f188(1);
        t = t + f189(1);
        t = t + f190(1);
        t = t + f191(1);
        t = t + f192(1);
        t = t + f193(1);
        t = t + f194(1);
        t = t + f195(1);
        t = t + f196(1);
        t = t + f197(1);
        t = t + f198(1);
        t = t + f199(1);
        t = t + f200(1);
        t = t + f201(1);
        t = t + f202(1);
        t = t + f203(1);
        t = t + f204(1);
        t = t + f205(1);
        t = t + f206(1);
        t = t + f207(1);
        t = t + f208(1);
        t = t + f209(1);
        t = t + f210(1);
        t = t + f211(1);
        t = t + f212(1);
        t = t + f213(1);
        t = t + f214(1);
        t = t + f215(1);
        t = t + f216(1);
        t = t + f217(1);
        t = t + f218(1);
        t = t + f219(1);
        t = t + f220(1);
        t = t + f221(1);
        t = t + f222(1);
        t = t + f223(1);
        t = t + f224(1);
        t = t + f225(1);
        t = t + f226(1);
        t = t + f227(1);
        t = t + f228(1);
        t = t + f229(1);
        t = t + f230(1);
        t = t + f231(1);
        t = t + f232(1);
        t = t + f233(1);
        t = t + f234(1);
        t = t + f235(1);
        t = t + f236(1);
        t = t + f237(1);
        t = t + f238(1);
        t = t + f239(1);
        t = t + f240(1);
        t = t + f241(1);
        t = t + f242(1);
        t = t + f243(1);
        t = t + f244(1);
        t = t + f245(1);
        t = t + f246(1);
        t = t + f247(1);
        t = t + f248(1);
        t = t + f249(1);
        t = t + f250(1);
        t = t + f251(1);
        t = t + f252(1);
        t = t + f253(1);
        t = t + f254(1);
        t = t + f255(1);
        t = t + f256(1);
        t = t + f257(1);
        t = t + f258(1);
        t = t + f259(1);
        t = t + f260(1);
        t = t + f261(1);
        t = t + f262(1);
        t = t + f263(1);
        t = t + f264(1);
        t = t + f265(1);
        t = t + f266(1);
        t = t + f267(1);
        t = t + f268(1);
        t = t + f269(1);
        t = t + f270(1);
        t = t + f271(1);
        t = t + f272(1);
        t = t + f273(1);
        t = t + f274(1);
        t = t + f275(1);
        t = t + f276(1);
        t = t + f277(1);
        t = t + f278(1);
        t = t + f279(1);
        t = t + f280(1);
        t = t + f281(1);
        t = t + f282(1);
        t = t + f283(1);
        t = t + f284(1);
        t = t + f285(1);
        t = t + f286(1);
        t = t + f287(1);
        t = t + f288(1);
        t = t + f289(1);
        t = t + f290(1);
        t = t + f291(1);
        t = t + f292(1);
        t = t + f293(1);
        t = t + f294(1);
        t = t + f295(1);
        t = t + f296(1);
        t = t + f297(1);
        t = t + f298(1);
        t = t + f299(1);
        int k = 10;
        Fn g = x -> x + k + f299(0);
        System.out.println(t);
        System.out.println(g.apply(1));
    }
}
