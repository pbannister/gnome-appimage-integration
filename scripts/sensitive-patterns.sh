#!/bin/sh
#
# sensitive-patterns.sh: the single source of the publish sanitization
# patterns. This file is sourced, not executed; scripts/leak-gate.sh and the
# tests read PATTERNS_SENSITIVE from it.
#
# Keep this the ONLY copy of the patterns. A duplicated pattern has already
# drifted once (one copy missed a fix), so never inline the pattern in another
# script or test.
#
# The pattern refuses:
#   - MAC addresses;
#   - private LAN addresses (RFC 1918), matched only as dotted quads, so a
#     numbered heading such as "## 10. Human Override" is not a false positive;
#   - the owner user names;
#   - absolute home paths;
#   - private-key material.

# The owner user names are the only project-specific part of the gate; a
# derived project edits this line.
PATTERNS_USER_NAMES=${PATTERNS_USER_NAMES:-preston|cainboy}

PATTERNS_SENSITIVE="([0-9a-fA-F]{2}:){5}[0-9a-fA-F]{2}|(^|[^0-9.])(192\.168\.|10\.[0-9]{1,3}\.|172\.(1[6-9]|2[0-9]|3[01])\.)|${PATTERNS_USER_NAMES}|/home/[a-zA-Z]|BEGIN (RSA |EC |OPENSSH |DSA )?PRIVATE KEY"
