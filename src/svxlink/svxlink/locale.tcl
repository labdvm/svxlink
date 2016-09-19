###############################################################################
#
# Locale specific functions for playing back time, numbers and spelling words.
# Often, the functions in this file are the only ones that have to be
# reimplemented for a new language pack.
#
###############################################################################

#
# Spell the specified word using the phonetic alphabet
#
proc spellWord_alpha {word} {
  set word [string tolower $word];
  for {set i 0} {$i < [string length $word]} {set i [expr $i + 1]} {
    set char [string index $word $i];
    if {$char == "*"} {
      playMsg "Default" "star";
    } elseif {$char == "/"} {
      playMsg "Default" "slash";
    } elseif {$char == "-"} {
      playMsg "Default" "dash";
    } elseif {[regexp {[a-z0-9]} $char]} {
      playMsg "Default" "phonetic_$char";
    }
  }
}

#
# Spell the specified word using the standard alphabet
#
proc spellWord {word} {
  set word [string tolower $word];
  for {set i 0} {$i < [string length $word]} {set i [expr $i + 1]} {
    set char [string index $word $i];
    if {$char == "*"} {
      playMsg "Default" "star";
    } elseif {$char == "/"} {
      playMsg "Default" "slash";
    } elseif {$char == "-"} {
      playMsg "Default" "dash";
    } elseif {[regexp {[a-z0-9]} $char]} {
      playMsg "Default" "$char";
    }
  }
}

#
# Spell the specified number digit for digit
#
proc spellNumber {number} {
  for {set i 0} {$i < [string length $number]} {set i [expr $i + 1]} {
    set ch [string index $number $i];
    if {$ch == "."} {
      playMsg "Default" "decimal";
    } else {
      playMsg "Default" "$ch";
      
    }
  }
}

#
# Say the specified two digit number (00 - 99)
#
proc playTwoDigitNumber {number} {
  if {[string length $number] != 2} {
    puts "*** WARNING: Function playTwoDigitNumber received a non two digit number: $number";
    return;
  }

set first [string index $number 0];
if {($first == "0") || ($first == "O")} {
    playMsg "Default" $first;
    playMsg "Default" [string index $number 1];
   }  elseif {$first == "1"} {
      playMsg "Default" $number;
   }  elseif {$first == "7"} {
      playMsg "Default" "6X";
      if {[string index $number 1] == "1"} {
        playMsg "Default" "and";
        }
          playMsg "Default" [expr $number - 60];
            } elseif {$first == "9"}  {
              playMsg "Default" "8X";
              playMsg "Default" [expr $number - 80];
            } else {
              playMsg "Default" "[string index $number 0]X";
            if {[string index $number 1] == "1"} {
              playMsg "Default" "and";
            }
            if {[string index $number 1] != "0"} {
             playMsg "Default" "[string index $number 1]";
        }
    }
}


#
# Say the specified three digit number (000 - 999)
#
proc playThreeDigitNumber {number} {
  if {[string length $number] != 3} {
    puts "*** WARNING: Function playThreeDigitNumber received a non three digit number: $number";
    return;
  }
  
  set first [string index $number 0];
  if {($first == "0") || ($first == "O")} {
    spellNumber $number
  } else {
    append first "00";
    playMsg "Default" $first;
    if {[string index $number 1] != "0"} {
      playMsg "Default" "and"
      playTwoDigitNumber [string range $number 1 2];
    } elseif {[string index $number 2] != "0"} {
      playMsg "Default" "and"
      playMsg "Default" [string index $number 2];
    }
  }
}

#
# Say a number as intelligent as posible. Examples:
#   x.x.x.x
#   xx.xx.xx.xx
#	xxx.xxx.xxx.xxx	- onehundred and xxx point xxx point xxx point xxx
#
proc playNumberIp {number} {
  if {[regexp {(\d+)\.(\d+).(\d+)\.(\d+)?} $number -> integer fraction]} {
    playNumber $integer;
    playMsg "Default" "decimal";
    spellNumber $fraction;
    return;
  }

  while {[string length $number] > 0} {
    set len [string length $number];
    if {$len == 1} {
      playMsg "Default" $number;
      set number "";
    } elseif {$len % 2 == 0} {
      playTwoDigitNumber [string range $number 0 1];
      set number [string range $number 2 end];
    } else {
      playThreeDigitNumber [string range $number 0 2];
      set number [string range $number 3 end];
    }
  }
}

#
# Say a number as intelligent as posible. Examples:
#
#	1	- one
#	24	- twentyfour
#	245	- twohundred and fourtyfive
#	1234	- twelve thirtyfour
#	12345	- onehundred and twentythree fourtyfive
#	136.5	- onehundred and thirtysix point five
#
proc playNumber {number} {
  if {[regexp {(\d+)\.(\d+)?} $number -> integer fraction]} {
    playNumber $integer;
    playMsg "Default" "decimal";
    spellNumber $fraction;
    return;
  }

  while {[string length $number] > 0} {
    set len [string length $number];
    if {$len == 1} {
      playMsg "Default" $number;
      set number "";
    } elseif {$len % 2 == 0} {
      playTwoDigitNumber [string range $number 0 1];
      set number [string range $number 2 end];
    } else {
      playThreeDigitNumber [string range $number 0 2];
      set number [string range $number 3 end];
    }
  }
}

#
# Say the time specified by function arguments "hour" and "minute".
#
proc playTime {hour minute} {
  # Strip white space and leading zeros. Check ranges.
  if {[scan $hour "%d" hour] != 1 || $hour < 0 || $hour > 23} {
    error "playTime: Non digit hour or value out of range: $hour"
  }
  if {[scan $minute "%d" minute] != 1 || $minute < 0 || $minute > 59} {
    error "playTime: Non digit minute or value out of range: $hour"
  }
  
  if {$hour < 12} {
    set ampm "AM";
    if {$hour == 0} {
      set hour 12;
    }
  } else {
    set ampm "PM";
    if {$hour > 12} {
      set hour [expr $hour - 12];
    }
  };
  
  playMsg "Default" [expr $hour];

  if {$minute != 0} {
    if {[string length $minute] == 1} {
      set minute "O$minute";
    }
    playTwoDigitNumber $minute;
  }
  
  playSilence 100;
  playMsg "Core" $ampm;
}

#
# Say temperature as intelligent as posible. Examples:
#
#       1       - one
#       24      - twentyfour
#       245     - twohundred and fourtyfive
#       1234    - twelve thirtyfour
#       12345   - onehundred and twentythree fourtyfive
#       136.5   - onehundred and thirtysix point five
#
proc playTemp {number} {
  if {[regexp {(\d+)\.(\d+)?} $number -> integer fraction]} {
    playNumber $integer;
    playMsg "WeatherStation" "degree";

# No say 0 if decimal = 0
if {$fraction != 0} {
    spellNumber $fraction;
}
   return;
}

  while {[string length $number] > 0} {
    set len [string length $number];
    if {$len == 1} {
      playMsg "Default" $number;
      set number "";
    } elseif {$len % 2 == 0} {
      playTwoDigitNumber [string range $number 0 1];
      set number [string range $number 2 end];
    } else {
      playThreeDigitNumber [string range $number 0 2];
      set number [string range $number 3 end];
    }
  }
}

#
# This file has not been truncated
#
