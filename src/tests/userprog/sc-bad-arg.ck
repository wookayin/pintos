# -*- perl -*-
use strict;
use warnings;
use tests::tests;
check_expected ([<<'EOF']);
(sc-bad-arg) begin
sc-bad-arg: exit(-1)
EOF
pass;
