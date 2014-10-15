# -*- perl -*-
use strict;
use warnings;
use tests::tests;
check_expected (IGNORE_USER_FAULTS => 1, IGNORE_EXIT_CODES => 1, [<<'EOF']);
(multi-oom) begin
(multi-oom) success. program forked 10 times.
(multi-oom) end
EOF
pass;
