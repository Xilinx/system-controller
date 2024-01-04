#
# Copyright (c) 2021 - 2022 Xilinx, Inc.  All rights reserved.
# Copyright (c) 2022 - 2024 Advanced Micro Devices, Inc.  All rights reserved.
#
# SPDX-License-Identifier: MIT
#

connect -xvc-url TCP:127.0.0.1:2542
targets -set -nocase -filter {name =~ "*Versal*"}

# The content of DNA registers varies for each part, however, currently
# 2 MSBs of DNA_3 register and 2 LSBs of DNA_0 register are defined to
# be non-zero.

set DNA0 [lindex [split [mrd 0xF1250020] ":"] 1]
set DNA3 [lindex [split [mrd 0xF125002C] ":"] 1]

set LSBs [expr 0x3 & [scan $DNA0 %x]]
set MSBs [expr [expr 0x3 << 30] & [scan $DNA3 %x]]

if {[scan $LSBs %x] == 0 || [scan $MSBs %x] == 0} {
    puts "FAIL"
    disconnect
    exit 0
}

puts "PASS"
disconnect
