# -*- perl -*-
use strict;
use warnings;
use tests::tests;
check_expected ([<<'EOF']);
(sc-bad-sp) begin
sc-bad-sp: exit(-1)
EOF
pass;
