#!/bin/sh
set -eu
cc -std=c99 -Wall -Wextra -Werror -pedantic src/minibox-printerd/http_body.c tests/test-http-body.c -o /tmp/test-http-body
/tmp/test-http-body
cc -std=c99 -Wall -Wextra -Werror -pedantic src/minibox-usb/stream.c src/minibox-ipp/print_job.c tests/test-print-job.c -o /tmp/test-print-job
/tmp/test-print-job
cc -std=c99 -Wall -Wextra -Werror -pedantic src/minibox-ipp/ipp.c tests/test-ipp.c -o /tmp/test-ipp
/tmp/test-ipp
cc -std=c99 -Wall -Wextra -Werror -pedantic src/minibox-ipp/ipp.c tests/test-ipp-format.c -o /tmp/test-ipp-format
/tmp/test-ipp-format
cc -std=c99 -Wall -Wextra -Werror -pedantic src/minibox-escl/escl.c tests/test-escl.c -o /tmp/test-escl
/tmp/test-escl
cc -std=c99 -Wall -Wextra -Werror -pedantic src/minibox-scan/soapht_transport.c tests/test-soapht-transport.c -o /tmp/test-soapht
/tmp/test-soapht
cc -std=c99 -Wall -Wextra -Werror -pedantic src/minibox-scan/soapht_transport.c src/minibox-scan/soapht_codec.c tests/test_soapht_codec.c -o /tmp/test-soapht-codec
/tmp/test-soapht-codec
cc -std=c99 -Wall -Wextra -Werror -pedantic src/minibox-scand/scan_session.c tests/test-scan-session.c -o /tmp/test-scan-session
/tmp/test-scan-session
cc -std=c99 -Wall -Wextra -Werror -pedantic src/minibox-scand/scan_backend.c tests/test-scan-backend.c -o /tmp/test-scan-backend
/tmp/test-scan-backend
echo 'MFP core contract OK'
