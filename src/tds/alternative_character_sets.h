/*
 * Copyright 2003 James K. Lowden <jklowden@schemamania.org>
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted, provided that redistributions of source 
 * code retain the above copyright notice.  
 */
 
 /* 
  * The purpose of this file is to help look up character set names.  
  * 
  * Any given encoding may be known by several (usually similar) aliases.  
  * For example, a system using ASCII encoding may report the character set as 
  * "ASCII", "US-ASCII", or "ISO646-US", among others.  For details on what your system 
  * uses, you may wish to consult the nl_langinfo(3) manual page.  
  *
  * GNU iconv converts a byte sequence from one encoding to another, but before it can do 
  * so, it must be told which is which.  In the list below, the preferred GNU iconv(3) name 
  * is on the left and an alias is on the right.  It is a simple exercise, left to the reader, 
  * to write a function that uses these data to look up the canonical name when provided 
  * an alias.  
  *
  * GNU iconv will in fact accept any name in the generated list, see comments below for details.  
  * Its complete set is included here to avoid duplication.  
  */

#ifndef _ALTERNATIVE_CHARACTER_SETS_H_
#define _ALTERNATIVE_CHARACTER_SETS_H_

#if 0
static char software_version[] = "$Id: alternative_character_sets.h,v 1.7 2003-05-14 15:02:08 freddy77 Exp $";
/*
To regenerate this file, do:
$ perl -x alternative_character_sets.h >/tmp/alternative_character_sets.h \
	&& mv /tmp/alternative_character_sets.h . 

The data between the "BEGIN ICONV" and "END ICONV" comments will be regenerated from "iconv -l".  
Please do not remove those comments, because the script below relies on them to know 
what to copy and what to ignore. 

Do not edit the data between "BEGIN ICONV" and "END ICONV"; your changes would be lost.  

The rest of the file, including additional rows at the end, is copied verbatim.  
# */

#! perl 
open MYSELF, $0 or die;

while(<MYSELF>) {
	last if(  m!^/\* \*\* BEGIN ICONV \*\* \*/! ) {
	print;
}

print "/* ** BEGIN ICONV ** */\n";
$comma = ' '; 
@names = `iconv -l`;
foreach( @names )  {
	my(@alternatives) = split;
	$canonical = $alternatives[0] or die;
	$canonical = qq/"$canonical",/;
	foreach my $name (@alternatives) {
		$name = qq/"$name"/;
		printf "\t${comma} {%20s %-25s}\n", $canonical, $name;
		$comma = ','; 
	}
}
print "/* **   END ICONV ** */\n";

while(<MYSELF>) {
	if(  m!^/\* \*\*   END ICONV \*\* \*/! ) {
		$printing++;
		next;
	}
	print if $printing;
}
__END__
#endif

/* ** BEGIN ICONV ** */
	  {         "US-ASCII", "US-ASCII"               }
	, {         "US-ASCII", "ANSI_X3.4-1968"         }
	, {         "US-ASCII", "ANSI_X3.4-1986"         }
	, {         "US-ASCII", "ASCII"                  }
	, {         "US-ASCII", "CP367"                  }
	, {         "US-ASCII", "IBM367"                 }
	, {         "US-ASCII", "ISO-IR-6"               }
	, {         "US-ASCII", "ISO646-US"              }
	, {         "US-ASCII", "ISO_646.IRV:1991"       }
	, {         "US-ASCII", "US"                     }
	, {         "US-ASCII", "CSASCII"                }
	, {            "UTF-8", "UTF-8"                  }
	, {            "UTF-8", "UTF8"                   }
	, {            "UTF-8", "utf8"                   }
	, {            "UCS-2", "UCS-2"                  }
	, {            "UCS-2", "UCS2"                   }
	, {            "UCS-2", "ucs2"                   }
	, {            "UCS-2", "ISO-10646-UCS-2"        }
	, {            "UCS-2", "CSUNICODE"              }
	, {            "UCS-2", "ucs2"                   }
	, {          "UCS-2BE", "UCS-2BE"                }
	, {          "UCS-2BE", "UNICODE-1-1"            }
	, {          "UCS-2BE", "UNICODEBIG"             }
	, {          "UCS-2BE", "CSUNICODE11"            }
	, {          "UCS-2LE", "UCS-2LE"                }
	, {          "UCS-2LE", "UNICODELITTLE"          }
	, {            "UCS-4", "UCS-4"                  }
	, {            "UCS-4", "UCS4"                   }
	, {            "UCS-4", "ISO-10646-UCS-4"        }
	, {            "UCS-4", "CSUCS4"                 }
	, {            "UCS-4", "ucs4"                   }
	, {          "UCS-4BE", "UCS-4BE"                }
	, {          "UCS-4LE", "UCS-4LE"                }
	, {           "UTF-16", "UTF-16"                 }
	, {           "UTF-16", "UTF16"                  }
	, {         "UTF-16BE", "UTF-16BE"               }
	, {         "UTF-16LE", "UTF-16LE"               }
	, {           "UTF-32", "UTF-32"                 }
	, {         "UTF-32BE", "UTF-32BE"               }
	, {         "UTF-32LE", "UTF-32LE"               }
	, {            "UTF-7", "UTF-7"                  }
	, {            "UTF-7", "UTF7"                   }
	, {            "UTF-7", "UNICODE-1-1-UTF-7"      }
	, {            "UTF-7", "CSUNICODE11UTF7"        }
	, {   "UCS-2-INTERNAL", "UCS-2-INTERNAL"         }
	, {    "UCS-2-SWAPPED", "UCS-2-SWAPPED"          }
	, {   "UCS-4-INTERNAL", "UCS-4-INTERNAL"         }
	, {    "UCS-4-SWAPPED", "UCS-4-SWAPPED"          }
	, {              "C99", "C99"                    }
	, {             "JAVA", "JAVA"                   }
	, {       "ISO-8859-1", "ISO-8859-1"             }
	, {       "ISO-8859-1", "ISO8859-1"              }
	, {       "ISO-8859-1", "CP819"                  }
	, {       "ISO-8859-1", "IBM819"                 }
	, {       "ISO-8859-1", "ISO-IR-100"             }
	, {       "ISO-8859-1", "ISO_8859-1"             }
	, {       "ISO-8859-1", "ISO_8859-1:1987"        }
	, {       "ISO-8859-1", "L1"                     }
	, {       "ISO-8859-1", "LATIN1"                 }
	, {       "ISO-8859-1", "CSISOLATIN1"            }
	, {       "ISO-8859-1", "iso88591"               }
	, {       "ISO-8859-1", "iso81"                  }
	, {       "ISO-8859-2", "ISO-8859-2"             }
	, {       "ISO-8859-2", "ISO-IR-101"             }
	, {       "ISO-8859-2", "ISO_8859-2"             }
	, {       "ISO-8859-2", "ISO_8859-2:1987"        }
	, {       "ISO-8859-2", "L2"                     }
	, {       "ISO-8859-2", "LATIN2"                 }
	, {       "ISO-8859-2", "CSISOLATIN2"            }
	, {       "ISO-8859-2", "iso88592"               }
	, {       "ISO-8859-2", "iso82"                  }
	, {       "ISO-8859-3", "ISO-8859-3"             }
	, {       "ISO-8859-3", "ISO-IR-109"             }
	, {       "ISO-8859-3", "ISO_8859-3"             }
	, {       "ISO-8859-3", "ISO_8859-3:1988"        }
	, {       "ISO-8859-3", "L3"                     }
	, {       "ISO-8859-3", "LATIN3"                 }
	, {       "ISO-8859-3", "CSISOLATIN3"            }
	, {       "ISO-8859-3", "iso88593"               }
	, {       "ISO-8859-3", "iso83"                  }
	, {       "ISO-8859-4", "ISO-8859-4"             }
	, {       "ISO-8859-4", "ISO-IR-110"             }
	, {       "ISO-8859-4", "ISO_8859-4"             }
	, {       "ISO-8859-4", "ISO_8859-4:1988"        }
	, {       "ISO-8859-4", "L4"                     }
	, {       "ISO-8859-4", "LATIN4"                 }
	, {       "ISO-8859-4", "CSISOLATIN4"            }
	, {       "ISO-8859-4", "iso88594"               }
	, {       "ISO-8859-4", "iso84"                  }
	, {       "ISO-8859-5", "ISO-8859-5"             }
	, {       "ISO-8859-5", "ISO8859-5"              }
	, {       "ISO-8859-5", "CYRILLIC"               }
	, {       "ISO-8859-5", "ISO-IR-144"             }
	, {       "ISO-8859-5", "ISO_8859-5"             }
	, {       "ISO-8859-5", "ISO_8859-5:1988"        }
	, {       "ISO-8859-5", "CSISOLATINCYRILLIC"     }
	, {       "ISO-8859-5", "iso88595"               }
	, {       "ISO-8859-5", "iso85"                  }
	, {       "ISO-8859-6", "ISO-8859-6"             }
	, {       "ISO-8859-6", "ISO8859-6"              }
	, {       "ISO-8859-6", "ARABIC"                 }
	, {       "ISO-8859-6", "ASMO-708"               }
	, {       "ISO-8859-6", "ECMA-114"               }
	, {       "ISO-8859-6", "ISO-IR-127"             }
	, {       "ISO-8859-6", "ISO_8859-6"             }
	, {       "ISO-8859-6", "ISO_8859-6:1987"        }
	, {       "ISO-8859-6", "CSISOLATINARABIC"       }
	, {       "ISO-8859-6", "iso88596"               }
	, {       "ISO-8859-6", "iso86"                  }
	, {       "ISO-8859-7", "ISO-8859-7"             }
	, {       "ISO-8859-7", "ISO8859-7"              }
	, {       "ISO-8859-7", "ECMA-118"               }
	, {       "ISO-8859-7", "ELOT_928"               }
	, {       "ISO-8859-7", "GREEK"                  }
	, {       "ISO-8859-7", "GREEK8"                 }
	, {       "ISO-8859-7", "ISO-IR-126"             }
	, {       "ISO-8859-7", "ISO_8859-7"             }
	, {       "ISO-8859-7", "ISO_8859-7:1987"        }
	, {       "ISO-8859-7", "CSISOLATINGREEK"        }
	, {       "ISO-8859-7", "iso88597"               }
	, {       "ISO-8859-7", "iso87"                  }
	, {       "ISO-8859-8", "ISO-8859-8"             }
	, {       "ISO-8859-8", "ISO8859-8"              }
	, {       "ISO-8859-8", "HEBREW"                 }
	, {       "ISO-8859-8", "ISO-IR-138"             }
	, {       "ISO-8859-8", "ISO_8859-8"             }
	, {       "ISO-8859-8", "ISO_8859-8:1988"        }
	, {       "ISO-8859-8", "CSISOLATINHEBREW"       }
	, {       "ISO-8859-8", "iso88598"               }
	, {       "ISO-8859-8", "iso88"                  }
	, {       "ISO-8859-9", "ISO-8859-9"             }
	, {       "ISO-8859-9", "ISO8859-9"              }
	, {       "ISO-8859-9", "ISO-IR-148"             }
	, {       "ISO-8859-9", "ISO_8859-9"             }
	, {       "ISO-8859-9", "ISO_8859-9:1989"        }
	, {       "ISO-8859-9", "L5"                     }
	, {       "ISO-8859-9", "LATIN5"                 }
	, {       "ISO-8859-9", "CSISOLATIN5"            }
	, {       "ISO-8859-9", "iso88599"               }
	, {       "ISO-8859-9", "iso89"                  }
	, {      "ISO-8859-10", "ISO-8859-10"            }
	, {      "ISO-8859-10", "ISO8859-10"             }
	, {      "ISO-8859-10", "ISO-IR-157"             }
	, {      "ISO-8859-10", "ISO_8859-10"            }
	, {      "ISO-8859-10", "ISO_8859-10:1992"       }
	, {      "ISO-8859-10", "L6"                     }
	, {      "ISO-8859-10", "LATIN6"                 }
	, {      "ISO-8859-10", "CSISOLATIN6"            }
	, {      "ISO-8859-13", "ISO-8859-13"            }
	, {      "ISO-8859-13", "ISO-IR-179"             }
	, {      "ISO-8859-13", "ISO_8859-13"            }
	, {      "ISO-8859-13", "L7"                     }
	, {      "ISO-8859-13", "LATIN7"                 }
	, {      "ISO-8859-14", "ISO-8859-14"            }
	, {      "ISO-8859-14", "ISO-CELTIC"             }
	, {      "ISO-8859-14", "ISO-IR-199"             }
	, {      "ISO-8859-14", "ISO_8859-14"            }
	, {      "ISO-8859-14", "ISO_8859-14:1998"       }
	, {      "ISO-8859-14", "L8"                     }
	, {      "ISO-8859-14", "LATIN8"                 }
	, {      "ISO-8859-15", "ISO-8859-15"            }
	, {      "ISO-8859-15", "ISO-IR-203"             }
	, {      "ISO-8859-15", "ISO_8859-15"            }
	, {      "ISO-8859-15", "ISO_8859-15:1998"       }
	, {      "ISO-8859-15", "iso885915"              }
	, {      "ISO-8859-15", "iso815"                 }
	, {      "ISO-8859-16", "ISO-8859-16"            }
	, {      "ISO-8859-16", "ISO-IR-226"             }
	, {      "ISO-8859-16", "ISO_8859-16"            }
	, {      "ISO-8859-16", "ISO_8859-16:2000"       }
	, {           "KOI8-R", "KOI8-R"                 }
	, {           "KOI8-R", "CSKOI8R"                }
	, {           "KOI8-U", "KOI8-U"                 }
	, {          "KOI8-RU", "KOI8-RU"                }
	, {           "CP1250", "CP1250"                 }
	, {           "CP1250", "MS-EE"                  }
	, {           "CP1250", "WINDOWS-1250"           }
	, {           "CP1250", "cp1250"                 }
	, {           "CP1251", "CP1251"                 }
	, {           "CP1251", "MS-CYRL"                }
	, {           "CP1251", "WINDOWS-1251"           }
	, {           "CP1251", "cp1251"                 }
	, {           "CP1252", "CP1252"                 }
	, {           "CP1252", "MS-ANSI"                }
	, {           "CP1252", "WINDOWS-1252"           }
	, {           "CP1252", "cp1252"                 }
	, {           "CP1253", "CP1253"                 }
	, {           "CP1253", "MS-GREEK"               }
	, {           "CP1253", "WINDOWS-1253"           }
	, {           "CP1253", "cp1253"                 }
	, {           "CP1254", "CP1254"                 }
	, {           "CP1254", "MS-TURK"                }
	, {           "CP1254", "WINDOWS-1254"           }
	, {           "CP1254", "cp1254"                 }
	, {           "CP1255", "CP1255"                 }
	, {           "CP1255", "MS-HEBR"                }
	, {           "CP1255", "WINDOWS-1255"           }
	, {           "CP1255", "cp1255"                 }
	, {           "CP1256", "CP1256"                 }
	, {           "CP1256", "MS-ARAB"                }
	, {           "CP1256", "WINDOWS-1256"           }
	, {           "CP1256", "cp1256"                 }
	, {           "CP1257", "CP1257"                 }
	, {           "CP1257", "WINBALTRIM"             }
	, {           "CP1257", "WINDOWS-1257"           }
	, {           "CP1257", "cp1257"                 }
	, {           "CP1258", "CP1258"                 }
	, {           "CP1258", "WINDOWS-1258"           }
	, {           "CP1258", "cp1258"                 }
	, {            "CP850", "CP850"                  }
	, {            "CP850", "850"                    }
	, {            "CP850", "IBM850"                 }
	, {            "CP850", "CSPC850MULTILINGUAL"    }
	, {            "CP850", "cp850"                  }
	, {            "CP862", "CP862"                  }
	, {            "CP862", "862"                    }
	, {            "CP862", "IBM862"                 }
	, {            "CP862", "CSPC862LATINHEBREW"     }
	, {            "CP862", "cp862"                  }
	, {            "CP866", "CP866"                  }
	, {            "CP866", "866"                    }
	, {            "CP866", "IBM866"                 }
	, {            "CP866", "CSIBM866"               }
	, {            "CP866", "cp866"                  }
	, {              "MAC", "MAC"                    }
	, {              "MAC", "MACINTOSH"              }
	, {              "MAC", "MACROMAN"               }
	, {              "MAC", "CSMACINTOSH"            }
	, { "MACCENTRALEUROPE", "MACCENTRALEUROPE"       }
	, {       "MACICELAND", "MACICELAND"             }
	, {      "MACCROATIAN", "MACCROATIAN"            }
	, {       "MACROMANIA", "MACROMANIA"             }
	, {      "MACCYRILLIC", "MACCYRILLIC"            }
	, {       "MACUKRAINE", "MACUKRAINE"             }
	, {         "MACGREEK", "MACGREEK"               }
	, {       "MACTURKISH", "MACTURKISH"             }
	, {        "MACHEBREW", "MACHEBREW"              }
	, {        "MACARABIC", "MACARABIC"              }
	, {          "MACTHAI", "MACTHAI"                }
	, {           "ROMAN8", "ROMAN8"                 }
	, {           "ROMAN8", "HP-ROMAN8"              }
	, {           "ROMAN8", "R8"                     }
	, {           "ROMAN8", "CSHPROMAN8"             }
	, {           "ROMAN8", "roman8"                 }
	, {           "ROMAN8", "roma8"                  }
	, {         "NEXTSTEP", "NEXTSTEP"               }
	, {        "ARMSCII-8", "ARMSCII-8"              }
	, { "GEORGIAN-ACADEMY", "GEORGIAN-ACADEMY"       }
	, {      "GEORGIAN-PS", "GEORGIAN-PS"            }
	, {           "KOI8-T", "KOI8-T"                 }
	, {        "MULELAO-1", "MULELAO-1"              }
	, {           "CP1133", "CP1133"                 }
	, {           "CP1133", "IBM-CP1133"             }
	, {       "ISO-IR-166", "ISO-IR-166"             }
	, {       "ISO-IR-166", "TIS-620"                }
	, {       "ISO-IR-166", "TIS620"                 }
	, {       "ISO-IR-166", "TIS620-0"               }
	, {       "ISO-IR-166", "TIS620.2529-1"          }
	, {       "ISO-IR-166", "TIS620.2533-0"          }
	, {       "ISO-IR-166", "TIS620.2533-1"          }
	, {       "ISO-IR-166", "tis620"                 }
	, {       "ISO-IR-166", "thai8"                  }
	, {            "CP874", "CP874"                  }
	, {            "CP874", "WINDOWS-874"            }
	, {            "CP874", "cp874"                  }
	, {           "VISCII", "VISCII"                 }
	, {           "VISCII", "VISCII1.1-1"            }
	, {           "VISCII", "CSVISCII"               }
	, {             "TCVN", "TCVN"                   }
	, {             "TCVN", "TCVN-5712"              }
	, {             "TCVN", "TCVN5712-1"             }
	, {             "TCVN", "TCVN5712-1:1993"        }
	, {        "ISO-IR-14", "ISO-IR-14"              }
	, {        "ISO-IR-14", "ISO646-JP"              }
	, {        "ISO-IR-14", "JIS_C6220-1969-RO"      }
	, {        "ISO-IR-14", "JP"                     }
	, {        "ISO-IR-14", "CSISO14JISC6220RO"      }
	, {    "JISX0201-1976", "JISX0201-1976"          }
	, {    "JISX0201-1976", "JIS_X0201"              }
	, {    "JISX0201-1976", "X0201"                  }
	, {    "JISX0201-1976", "CSHALFWIDTHKATAKANA"    }
	, {        "ISO-IR-87", "ISO-IR-87"              }
	, {        "ISO-IR-87", "JIS0208"                }
	, {        "ISO-IR-87", "JIS_C6226-1983"         }
	, {        "ISO-IR-87", "JIS_X0208"              }
	, {        "ISO-IR-87", "JIS_X0208-1983"         }
	, {        "ISO-IR-87", "JIS_X0208-1990"         }
	, {        "ISO-IR-87", "X0208"                  }
	, {        "ISO-IR-87", "CSISO87JISX0208"        }
	, {       "ISO-IR-159", "ISO-IR-159"             }
	, {       "ISO-IR-159", "JIS_X0212"              }
	, {       "ISO-IR-159", "JIS_X0212-1990"         }
	, {       "ISO-IR-159", "JIS_X0212.1990-0"       }
	, {       "ISO-IR-159", "X0212"                  }
	, {       "ISO-IR-159", "CSISO159JISX02121990"   }
	, {               "CN", "CN"                     }
	, {               "CN", "GB_1988-80"             }
	, {               "CN", "ISO-IR-57"              }
	, {               "CN", "ISO646-CN"              }
	, {               "CN", "CSISO57GB1988"          }
	, {          "CHINESE", "CHINESE"                }
	, {          "CHINESE", "GB_2312-80"             }
	, {          "CHINESE", "ISO-IR-58"              }
	, {          "CHINESE", "CSISO58GB231280"        }
	, {          "CHINESE", "hp15CN"                 }
	, {   "CN-GB-ISOIR165", "CN-GB-ISOIR165"         }
	, {   "CN-GB-ISOIR165", "ISO-IR-165"             }
	, {       "ISO-IR-149", "ISO-IR-149"             }
	, {       "ISO-IR-149", "KOREAN"                 }
	, {       "ISO-IR-149", "KSC_5601"               }
	, {       "ISO-IR-149", "KS_C_5601-1987"         }
	, {       "ISO-IR-149", "KS_C_5601-1989"         }
	, {       "ISO-IR-149", "CSKSC56011987"          }
	, {           "EUC-JP", "EUC-JP"                 }
	, {           "EUC-JP", "EUCJP"                  }
	, {           "EUC-JP", "EXTENDED_UNIX_CODE_PACKED_FORMAT_FOR_JAPANESE"}
	, {           "EUC-JP", "CSEUCPKDFMTJAPANESE"    }
	, {           "EUC-JP", "eucJP"                  }
	, {             "SJIS", "SJIS"                   }
	, {             "SJIS", "MS_KANJI"               }
	, {             "SJIS", "SHIFT-JIS"              }
	, {             "SJIS", "SHIFT_JIS"              }
	, {             "SJIS", "CSSHIFTJIS"             }
	, {             "SJIS", "sjis"                   }
	, {            "CP932", "CP932"                  }
	, {      "ISO-2022-JP", "ISO-2022-JP"            }
	, {      "ISO-2022-JP", "CSISO2022JP"            }
	, {    "ISO-2022-JP-1", "ISO-2022-JP-1"          }
	, {    "ISO-2022-JP-2", "ISO-2022-JP-2"          }
	, {    "ISO-2022-JP-2", "CSISO2022JP2"           }
	, {            "CN-GB", "CN-GB"                  }
	, {            "CN-GB", "EUC-CN"                 }
	, {            "CN-GB", "EUCCN"                  }
	, {            "CN-GB", "GB2312"                 }
	, {            "CN-GB", "CSGB2312"               }
	, {            "CP936", "CP936"                  }
	, {            "CP936", "GBK"                    }
	, {          "GB18030", "GB18030"                }
	, {      "ISO-2022-CN", "ISO-2022-CN"            }
	, {      "ISO-2022-CN", "CSISO2022CN"            }
	, {  "ISO-2022-CN-EXT", "ISO-2022-CN-EXT"        }
	, {               "HZ", "HZ"                     }
	, {               "HZ", "HZ-GB-2312"             }
	, {           "EUC-TW", "EUC-TW"                 }
	, {           "EUC-TW", "EUCTW"                  }
	, {           "EUC-TW", "CSEUCTW"                }
	, {           "EUC-TW", "eucTW"                  }
	, {            "BIG-5", "BIG-5"                  }
	, {            "BIG-5", "BIG-FIVE"               }
	, {            "BIG-5", "BIG5"                   }
	, {            "BIG-5", "BIGFIVE"                }
	, {            "BIG-5", "CN-BIG5"                }
	, {            "BIG-5", "CSBIG5"                 }
	, {            "BIG-5", "big5"                   }
	, {            "CP950", "CP950"                  }
	, {       "BIG5-HKSCS", "BIG5-HKSCS"             }
	, {       "BIG5-HKSCS", "BIG5HKSCS"              }
	, {           "EUC-KR", "EUC-KR"                 }
	, {           "EUC-KR", "EUCKR"                  }
	, {           "EUC-KR", "CSEUCKR"                }
	, {           "EUC-KR", "eucKR"                  }
	, {            "CP949", "CP949"                  }
	, {            "CP949", "UHC"                    }
	, {           "CP1361", "CP1361"                 }
	, {           "CP1361", "JOHAB"                  }
	, {      "ISO-2022-KR", "ISO-2022-KR"            }
	, {      "ISO-2022-KR", "CSISO2022KR"            }
/* **   END ICONV ** */
	/*
	 * Add OS character set names here.  
	 * The first string is the GNU iconv canonical name.  
	 * The second element is whatever string is returned by nl_langinfo.  
	 */
	/* NetBSD */
	, {         "US-ASCII", "646"                    }	/* NetBSD default */
	/* other operating systems */
	
	/* no stopper row; add your own */
#endif
