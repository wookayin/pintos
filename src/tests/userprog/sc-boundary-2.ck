# -*- perl -*-
use strict;
use warnings;
use tests::tests;
check_expected ([<<'EOF']);
(sc-boundary-2) begin
sc-boundary-2: exit(67)
EOF
pass;
