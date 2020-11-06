#!/bin/bash

set -euo pipefail

git archive --format tgz -o "$1" HEAD

