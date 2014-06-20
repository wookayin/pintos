# -*- perl -*-
use strict;
use warnings;
use tests::tests;
use tests::vm::process_death;

check_process_death ('mmap-unmap');
