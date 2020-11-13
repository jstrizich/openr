#!/usr/bin/env python3

#
# Copyright (c) 2014-present, Facebook, Inc.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.
#

import logging
import os
from pathlib import Path
from subprocess import check_call
from sys import version_info


INSTALL_BASE_PATH = Path("/opt/facebook")
LOG = logging.getLogger(__name__)
# Set for Ubuntu Docker Container
# e.g. /usr/local/lib/python3.8/dist-packages/openr/
PY_PACKAGE_DIR = "/usr/local/lib/python{}.{}/dist-packages/".format(
    version_info.major, version_info.minor
)


def generate_thrift_files():
    """
    Get list of all thrift files (absolute path names) and then generate
    python definitions for all thrift files.
    """

    install_base = str(INSTALL_BASE_PATH)
    if "OPENR_INSTALL_BASE" in os.environ:
        install_base = os.environ["OPENR_INSTALL_BASE"]

    include_paths = (
        install_base,
        "{}/fb303/include/thrift-files/".format(install_base),
    )
    include_args = []
    for include_path in include_paths:
        include_args.extend(["-I", include_path])

    # /src/py/
    setup_py_path = Path(__file__).parent.absolute()
    # /src
    openr_root_path = setup_py_path.parent
    # /src/if/
    thrift_path = openr_root_path / "if"

    # Skip building files for py and only for py3
    py_exclude_files = ["OpenrCtrlCpp"]

    thrift_files = sorted(thrift_path.rglob("*.thrift"))
    LOG.info("Going to build the following thrift files from {}: {}".format(
        str(thrift_path), thrift_files)
    )
    for thrift_file in thrift_files:
        for thrift_lang in ("py", "py3"):
            if thrift_lang == "py" and thrift_file.stem in py_exclude_files:
                LOG.info("Skipping {} py thrift compile".format(thrift_file))
                continue

            LOG.info("> Generating python definition for {} in {}".format(
                thrift_file, thrift_lang)
            )
            check_call(
                [
                    "thrift1",
                    "--gen",
                    thrift_lang,
                    "-I",
                    str(openr_root_path),
                    *include_args,
                    "--out",
                    PY_PACKAGE_DIR,
                    str(thrift_file),
                ]
            )
    
    # Symlink fbthrift python into PY_PACKAGE_DIR
    fb_thrift_path = INSTALL_BASE_PATH / "fbthrift/lib/fb-py-libs/thrift_py/thrift"
    thrift_py_package_path = Path(PY_PACKAGE_DIR) / "thrift"
    thrift_py_package_path.symlink_to(fb_thrift_path)

    # Symlink fb303 python into PY_PACKAGE_DIR
    fb_fb303_path = INSTALL_BASE_PATH / "fb303/lib/fb-py-libs/fb303_thrift_py/fb303_core"
    fb303_py_pacakge_path = Path(PY_PACKAGE_DIR) / "fb303_core"
    fb303_py_pacakge_path.symlink_to(fb_fb303_path)


if __name__ == "__main__":
    logging.basicConfig(
        format="[%(asctime)s] %(levelname)s: %(message)s (%(filename)s:%(lineno)d)",
        level=logging.DEBUG,
    )
    LOG.info("Generating Open/R Thrift Libraries")
    generate_thrift_files()  # pragma: no cover