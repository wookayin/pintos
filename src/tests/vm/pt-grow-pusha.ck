# -*- perl -*-
use strict;
use warnings;
use tests::tests;
check_expected (IGNORE_EXIT_CODES => 1, [<<'EOF']);
(pt-grow-pusha) begin
(pt-grow-pusha) end
EOF
pass;
