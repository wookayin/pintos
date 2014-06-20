# -*- perl -*-
use strict;
use warnings;
use tests::tests;
check_expected ([<<'EOF', <<'EOF', <<'EOF', <<'EOF']);
(exec-missing) begin
load: no-such-file: open failed
(exec-missing) exec("no-such-file"): -1
(exec-missing) end
exec-missing: exit(0)
EOF
(exec-missing) begin
(exec-missing) exec("no-such-file"): -1
(exec-missing) end
exec-missing: exit(0)
EOF
(exec-missing) begin
load: no-such-file: open failed
no-such-file: exit(-1)
(exec-missing) exec("no-such-file"): -1
(exec-missing) end
exec-missing: exit(0)
EOF
(exec-missing) begin
load: no-such-file: open failed
(exec-missing) exec("no-such-file"): -1
no-such-file: exit(-1)
(exec-missing) end
exec-missing: exit(0)
EOF
pass;
