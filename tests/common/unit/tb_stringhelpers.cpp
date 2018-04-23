// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 


#include <string>
#include "gtest/gtest.h"

#include "common/StringHelpers.hh"

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
    for(int i=0; i<offsets.size(); i++) {
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
    for(int i=0; i<offsets.size(); i++) {
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
    for(int i=0; i<offsets.size(); i++) {
      cout <<offsets[i]<<"\t"<<hex_lines[i]<<"\t"<<txt_lines[i]<<endl;
    }
  }

}
