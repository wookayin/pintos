# -*- perl -*-
use strict;
use warnings;
use tests::tests;
check_expected ([<<'EOF']);
(sc-boundary) begin
sc-boundary: exit(42)
EOF
pass;
