// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.


#include <string>

#include <wordexp.h>

#include "gtest/gtest.h"

#include "faodel-common/StringHelpers.hh"

#define DEBUG (0)

using namespace std;
using namespace faodel;

TEST(StringHelpers, SplitBasic) {
  string s1="this,is,c,s,v,data";
  vector<string> tokens;
  Split(tokens, s1, ',',false);
  EXPECT_EQ(6,tokens.size());
  EXPECT_EQ("this", tokens[0]);
  EXPECT_EQ("is",   tokens[1]);
  EXPECT_EQ("c",    tokens[2]);
  EXPECT_EQ("s",    tokens[3]);
  EXPECT_EQ("v",    tokens[4]);
  EXPECT_EQ("data", tokens[5]);

  string s2="this,,has,some,,missing,data,";
  vector<string> t2;
  Split(t2, s2, ',', false);
  EXPECT_EQ(8, t2.size());
  EXPECT_EQ("", t2[1]);
  EXPECT_EQ("some", t2[3]);
  EXPECT_EQ("data",t2[6]);
  EXPECT_EQ("", t2[7]);

  vector<string> t3;
  Split(t3, s2, ',', true); //Remove gaps
  EXPECT_EQ(5, t3.size());
  EXPECT_EQ("this",    t3[0]);
  EXPECT_EQ("has",     t3[1]);
  EXPECT_EQ("some",    t3[2]);
  EXPECT_EQ("missing", t3[3]);
  EXPECT_EQ("data",    t3[4]);

}

TEST(StringHelpers, ToLowerUpper) {
  string s="ThIs Is LoWeR 123";
  EXPECT_EQ("this is lower 123", ToLowercase(s));
  ToLowercaseInPlace(s);
  EXPECT_EQ("this is lower 123", s);
}

TEST(StringHelpers, BeginsWith) {
  string prefix = "This";
  string good[] = {"This", "This is a big test", "This should match"};
  string bad[] = {"Th", "th", "this", "", "Thiz is" };

  for(auto s : good) {
    bool hit = StringBeginsWith(s, prefix);
    EXPECT_TRUE(hit);
  }

  for(auto s : bad) {
    bool hit = StringBeginsWith(s, prefix);
    EXPECT_FALSE(hit);
  }
}

TEST(StringHelpers, EndsWith) {
  string suffix = ".exe";
  string good[] = {"file.exe", "This is a big test.exe", ".exe"};
  string bad[] = {"X", "exe", ".EXE", "", "Thiz is file.Exe" };

  for(auto s : good) {
    bool hit = StringEndsWith(s, suffix);
    EXPECT_TRUE(hit);
  }

  for(auto s : bad) {
    bool hit = StringEndsWith(s, suffix);
    EXPECT_FALSE(hit);
  }
}

TEST(StringHelpers, ExpandPathWithFlags) {
    char *penv = getenv("HOME");
    EXPECT_TRUE(penv!=nullptr);

    string s1("~");
    string expanded1 = ExpandPath(s1, 0);
    EXPECT_EQ(penv, expanded1);

    string s2("$(echo ~)");
    string expanded2 = ExpandPath(s2, WRDE_NOCMD);
    EXPECT_EQ("", expanded2);

    string s3("$UNDEF");
    string expanded3 = ExpandPath(s3, WRDE_UNDEF);
    EXPECT_EQ("", expanded3);
}

TEST(StringHelpers, ExpandPath) {
    char *penv = getenv("HOME");
    EXPECT_TRUE(penv!=nullptr);

    string s1("~");
    string expanded1 = ExpandPath(s1);
    EXPECT_EQ(penv, expanded1);

    string s2("$(echo ~)");
    string expanded2 = ExpandPath(s2);
    EXPECT_EQ(penv, expanded2);

    string s3("$HOME");
    string expanded3 = ExpandPath(s3);
    EXPECT_EQ(penv, expanded3);
}

TEST(StringHelpers, ExpandPathSafely) {
    char *penv = getenv("HOME");
    EXPECT_TRUE(penv!=nullptr);

    string s1("~");
    string expanded1 = ExpandPathSafely(s1);
    EXPECT_EQ(penv, expanded1);

    string s2("$(echo ~)");
    string expanded2 = ExpandPathSafely(s2);
    EXPECT_EQ("", expanded2);

    string s3("$HOME");
    string expanded3 = ExpandPathSafely(s3);
    EXPECT_EQ(penv, expanded3);
}

TEST(StringHelpers, HexDumpBasic) {

  char x[32];
  for(int i=0; i<32; i++)
    x[i]=i&0x0ff;
  x[0]='t';
  x[1]='e';
  x[2]='s';
  x[3]='t';

  vector<string> offsets;
  vector<string> hex_lines;
  vector<string> txt_lines;
  ConvertToHexDump(x, 32, 8, 4, "", "", "","",
                   &offsets, &hex_lines, &txt_lines);

  EXPECT_EQ(4, offsets.size());
  EXPECT_EQ(4, hex_lines.size());
  EXPECT_EQ(4, txt_lines.size());

  EXPECT_EQ("0", offsets[0]);
  EXPECT_EQ("8", offsets[1]);
  EXPECT_EQ("16", offsets[2]);
  EXPECT_EQ("24", offsets[3]);

  EXPECT_EQ("74 65 73 74 04 05 06 07", hex_lines[0]);
  EXPECT_EQ("08 09 0A 0B 0C 0D 0E 0F", hex_lines[1]);
  EXPECT_EQ("10 11 12 13 14 15 16 17", hex_lines[2]);
  EXPECT_EQ("18 19 1A 1B 1C 1D 1E 1F", hex_lines[3]);

  EXPECT_EQ("test....", txt_lines[0]);
  for(int i=1; i<3; i++)
    EXPECT_EQ("........", txt_lines[i]);

  if(DEBUG) {
    for(size_t i=0; i<offsets.size(); i++) {
      cout <<offsets[i]<<"\t"<<hex_lines[i]<<"\t"<<txt_lines[i]<<endl;
    }
  }

}

TEST(StringHelpers, HexDumpSplit) {

  char x[36];
  for(int i=0; i<36; i++)
    x[i]=i&0x0ff;
  x[0]='t';
  x[1]='e';
  x[2]='s';
  x[3]='t';

  vector<string> offsets;
  vector<string> hex_lines;
  vector<string> txt_lines;
  ConvertToHexDump(x, 36, 8, 2, "<", ">", "(",")",
                   &offsets, &hex_lines, &txt_lines);

  EXPECT_EQ(5, offsets.size());
  EXPECT_EQ(5, hex_lines.size());
  EXPECT_EQ(5, txt_lines.size());

  EXPECT_EQ("0", offsets[0]);
  EXPECT_EQ("8", offsets[1]);
  EXPECT_EQ("16", offsets[2]);
  EXPECT_EQ("24", offsets[3]);
  EXPECT_EQ("32", offsets[4]);

  EXPECT_EQ("<74 65 >(73 74 )<04 05 >(06 07)", hex_lines[0]);
  EXPECT_EQ("<08 09 >(0A 0B )<0C 0D >(0E 0F)", hex_lines[1]);
  EXPECT_EQ("<10 11 >(12 13 )<14 15 >(16 17)", hex_lines[2]);
  EXPECT_EQ("<18 19 >(1A 1B )<1C 1D >(1E 1F)", hex_lines[3]);
  EXPECT_EQ("<20 21 >(22 23 )<>()",            hex_lines[4]); //Two empty spots at end

  EXPECT_EQ("<te>(st)<..>(..)", txt_lines[0]);
  for(int i=1; i<3; i++)
    EXPECT_EQ("<..>(..)<..>(..)", txt_lines[i]);
  EXPECT_EQ("< !>(\"#)<>()", txt_lines[4]);

  if(DEBUG) {
    for(size_t i=0; i<offsets.size(); i++) {
      cout <<offsets[i]<<"\t"<<hex_lines[i]<<"\t"<<txt_lines[i]<<endl;
    }
  }

}

TEST(StringHelpers, HexDumpHTML) {

  char x[32];
  for(int i=0; i<32; i++)
    x[i]=i&0x0ff;
  x[0]='t';
  x[1]='e';
  x[2]='s';
  x[3]='t';

  vector<string> offsets;
  vector<string> hex_lines;
  vector<string> txt_lines;
  ConvertToHexDump(x, 32, 8, 4,
                   "<span class=\"HEXE\">", "</span>",
                   "<span class=\"HEXO\">", "</span>",
                   &offsets, &hex_lines, &txt_lines);

  EXPECT_EQ(4, offsets.size());
  EXPECT_EQ(4, hex_lines.size());
  EXPECT_EQ(4, txt_lines.size());

  EXPECT_EQ("0", offsets[0]);
  EXPECT_EQ("8", offsets[1]);
  EXPECT_EQ("16", offsets[2]);
  EXPECT_EQ("24", offsets[3]);

  EXPECT_EQ("<span class=\"HEXE\">74 65 73 74 </span><span class=\"HEXO\">04 05 06 07</span>", hex_lines[0]);
  EXPECT_EQ("<span class=\"HEXE\">08 09 0A 0B </span><span class=\"HEXO\">0C 0D 0E 0F</span>", hex_lines[1]);
  EXPECT_EQ("<span class=\"HEXE\">10 11 12 13 </span><span class=\"HEXO\">14 15 16 17</span>", hex_lines[2]);
  EXPECT_EQ("<span class=\"HEXE\">18 19 1A 1B </span><span class=\"HEXO\">1C 1D 1E 1F</span>", hex_lines[3]);

  EXPECT_EQ("<span class=\"HEXE\">test</span><span class=\"HEXO\">....</span>", txt_lines[0]);
  for(int i=1; i<3; i++)
    EXPECT_EQ("<span class=\"HEXE\">....</span><span class=\"HEXO\">....</span>", txt_lines[i]);

  if(DEBUG) {
    for(size_t i=0; i<offsets.size(); i++) {
      cout <<offsets[i]<<"\t"<<hex_lines[i]<<"\t"<<txt_lines[i]<<endl;
    }
  }

}

TEST(StringHelpers, RangeParsing) {

  set<int> x; set<int> exp;

  //Good info
  x = ExtractIDs("1", 8); exp={1}; EXPECT_EQ( exp, x);
  x = ExtractIDs("1,3",8); exp={1,3}; EXPECT_EQ( exp, x);
  x = ExtractIDs("1,3,5,7",8); exp={1,3,5,7}; EXPECT_EQ( exp, x);
  x = ExtractIDs("2-5",8); exp={2,3,4,5}; EXPECT_EQ( exp, x);
  x = ExtractIDs("2-5,4-6",8); exp={2,3,4,5,6}; EXPECT_EQ( exp, x);
  x = ExtractIDs("1-2,4-5,7-8",9); exp={1,2,4,5,7,8}; EXPECT_EQ( exp, x);
  x = ExtractIDs("4-5,1,3,8",9); exp={1,3,4,5,8}; EXPECT_EQ( exp, x);
  x = ExtractIDs("0-0",2); exp={0}; EXPECT_EQ( exp, x);

  //Good - with spaces
  x = ExtractIDs("4-5, 1, 3 , 8",9); exp={1,3,4,5,8}; EXPECT_EQ( exp, x);
  x = ExtractIDs("1 - 2, 4 - 5, 7- 8",9); exp={1,2,4,5,7,8}; EXPECT_EQ( exp, x);

  //Good - using words
  x = ExtractIDs("all",4); exp={0,1,2,3}; EXPECT_EQ( exp,x);
  x = ExtractIDs("end",4); exp={3}; EXPECT_EQ( exp,x);
  x = ExtractIDs("middle",4); exp={1}; EXPECT_EQ( exp,x);
  x = ExtractIDs("middle",5); exp={2}; EXPECT_EQ( exp,x);
  x = ExtractIDs("middle-end",4); exp={1,2,3}; EXPECT_EQ( exp,x);
  x = ExtractIDs("0-middle,end",4); exp={0,1,3}; EXPECT_EQ( exp,x);
  x = ExtractIDs("middleplus,end",4); exp={2,3}; EXPECT_EQ( exp, x);
  x = ExtractIDs("all,0",4); exp={0,1,2,3}; EXPECT_EQ( exp,x);
  x = ExtractIDs("0-middle",2); exp={0}; EXPECT_EQ( exp,x);

  //Bad values
  EXPECT_ANY_THROW(ExtractIDs("1", 1));
  EXPECT_ANY_THROW(ExtractIDs("3-5", 4));
  EXPECT_ANY_THROW(ExtractIDs("2-1", 4));
  EXPECT_ANY_THROW(ExtractIDs("1,2-10", 8));
  EXPECT_ANY_THROW(ExtractIDs("1,middle-10", 8));

  EXPECT_ANY_THROW(ExtractIDs("fishbone",10));
  EXPECT_ANY_THROW(ExtractIDs("1,2,3 4,5",10));

}

TEST(PunyCode, Basics) {
  const string src="This is the input/output that I ~want to store!!";
  string s="This is it";

  string enc1 = MakePunycode(src);
  string dec1 = ExpandPunycode(enc1);
  string enc2 = MakePunycode(dec1);
  string dec2 = ExpandPunycode(enc2);

  EXPECT_EQ(src,dec1);
  EXPECT_EQ(src,dec2);
  EXPECT_EQ(enc1,enc2);
  EXPECT_NE(src,enc1);
}

TEST(PunyCode, ZeroVals) {

  //Put a zero in the middle of it.
  //Needed because kelpie sometimes encodes a key's lengths into
  //a string as chars.
  string s="The end";
  s[3]=0;
  EXPECT_EQ("The%00end", MakePunycode(s));

}

TEST(PunyCode, RawData) {
  //Build a string with chars 1-255
  stringstream ss;
  for(unsigned int i=0;i<255; i++) {
    char x = (i+1)&0x0FF;
    ss << x;
  }
  string src = ss.str();
  string enc = MakePunycode(src);
  string dec = ExpandPunycode(enc);

  EXPECT_EQ(src,dec);
  EXPECT_NE(src,enc);
}

TEST(StringToNum, Basics) {
  int rc;
  uint64_t val;

  //Good
  rc = StringToTimeUS(&val, "9"); EXPECT_EQ(0,rc); EXPECT_EQ(9, val);
  rc = StringToTimeUS(&val, "100us"); EXPECT_EQ(0,rc); EXPECT_EQ(100, val);
  rc = StringToTimeUS(&val, "6 us"); EXPECT_EQ(0,rc); EXPECT_EQ(6, val);
  rc = StringToTimeUS(&val, "82ms"); EXPECT_EQ(0,rc); EXPECT_EQ(82000, val);
  rc = StringToTimeUS(&val, "3s"); EXPECT_EQ(0,rc); EXPECT_EQ(3000000ul, val);
  rc = StringToTimeUS(&val, "400 Seconds"); EXPECT_EQ(0,rc); EXPECT_EQ(400*1000000, val);
  rc = StringToTimeUS(&val, "5minutes"); EXPECT_EQ(0,rc); EXPECT_EQ(5*60*1000000ul, val);
  rc = StringToTimeUS(&val, "2 hours"); EXPECT_EQ(0,rc); EXPECT_EQ(2*3600*1000000ul, val);

  //Bad
  rc = StringToTimeUS(&val, "hours"); EXPECT_EQ(EINVAL,rc); EXPECT_EQ(0, val);
  rc = StringToTimeUS(&val, "hour"); EXPECT_EQ(EINVAL,rc); EXPECT_EQ(0, val);
  rc = StringToTimeUS(&val, "9x46minutes"); EXPECT_EQ(EINVAL, rc); EXPECT_EQ(0UL, val);


}