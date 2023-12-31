# Copyright 2021 The Pigweed Authors
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may not
# use this file except in compliance with the License. You may obtain a copy of
# the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations under
# the License.
"""Shell related checks."""

import logging

from pw_presubmit import presubmit_context
from pw_presubmit.presubmit import (
    Check,
    filter_paths,
)
from pw_presubmit.presubmit_context import (
    PresubmitContext,
    PresubmitFailure,
)
from pw_presubmit.tools import log_run

_LOG = logging.getLogger(__name__)

_SHELL_EXTENSIONS = ('.sh', '.bash')


@filter_paths(endswith=_SHELL_EXTENSIONS)
@Check
def shellcheck(ctx: PresubmitContext) -> None:
    """Run shell script static analiyzer on presubmit."""

    ctx.paths = presubmit_context.apply_exclusions(ctx)

    if ctx.paths:
        _LOG.warning(
            "The Pigweed project discourages use of shellscripts. "
            "https://pigweed.dev/docs/faq.html"
        )

    result = log_run(['shellcheck', *ctx.paths])
    if result.returncode != 0:
        raise PresubmitFailure('Shellcheck identifed issues.')
