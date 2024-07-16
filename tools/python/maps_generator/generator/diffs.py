from pathlib import Path

import subprocess
import warnings

class Status:
    NO_NEW_VERSION = "Failed: new version doesn't exist: {new}"
    INTERNAL_ERROR = "Failed: internal error (C++ module) while calculating"

    NO_OLD_VERSION = "Skipped: old version doesn't exist: {old}"
    NOTHING_TO_DO = "Skipped: output already exists: {out}"
    OK = "Succeeded: calculated {out}: {diff_size} out of {new_size} bytes"
    TOO_LARGE = "Cancelled: {out}: diff {diff_size} > new version {new_size}"

    @classmethod
    def is_error(cls, status):
        return status == cls.NO_NEW_VERSION or status == cls.INTERNAL_ERROR


def calculate_diff(params):
    new, old, out = params["new"], params["old"], params["out"]

    diff_size = 0

    if not new.exists():
        return Status.NO_NEW_VERSION, params

    if not old.exists():
        return Status.NO_OLD_VERSION, params

    status = Status.OK
    if out.exists():
        status = Status.NOTHING_TO_DO
    else:
        # FIXME: running as a single string without shell=True doesn't work (did it ever?) and mwm_diff_tool is actually pymwm_diff CMAKE target
        res = subprocess.run(["mwm_diff_tool", "make", old.as_posix(), new.as_posix(), out.as_posix()],
            stderr=subprocess.STDOUT, stdout=subprocess.PIPE)
        if res.returncode != 0:
            # TODO: better handling of tool errors
            print(res.stdout)
            return Status.INTERNAL_ERROR, params

    diff_size = out.stat().st_size
    new_size = new.stat().st_size

    if diff_size > new_size:
        status = Status.TOO_LARGE

    params.update({
        "diff_size": diff_size,
        "new_size": new_size
    })

    return status, params


def mwm_diff_calculation(data_dir, logger, depth):
    data = list(data_dir.get_mwms())[:depth]

    for params in data:
        status, params = calculate_diff(params)
        if Status.is_error(status):
            raise Exception(status.format(**params))
        logger.info(status.format(**params))


class DataDir(object):
    def __init__(self, country_name, new_version_dir, old_version_root_dir):
        self.mwm_name = country_name + ".mwm"
        self.diff_name = country_name + ".mwmdiff"

        self.new_version_dir = Path(new_version_dir)
        self.new_version_path = Path(new_version_dir, self.mwm_name)
        self.old_version_root_dir = Path(old_version_root_dir)

    def get_mwms(self):
        old_versions = sorted(
            self.old_version_root_dir.glob("[0-9]*"),
            reverse=True
        )
        for old_version_dir in old_versions:
            if (old_version_dir != self.new_version_dir and
                    old_version_dir.is_dir()):
                diff_dir = Path(self.new_version_dir, old_version_dir.name)
                diff_dir.mkdir(exist_ok=True)
                yield {
                    "new": self.new_version_path,
                    "old": Path(old_version_dir, self.mwm_name),
                    "out": Path(diff_dir, self.diff_name)
                }


if __name__ == "__main__":
    import logging
    import sys

    logger = logging.getLogger()
    logger.addHandler(logging.StreamHandler(stream=sys.stdout))
    logger.setLevel(logging.DEBUG)

    data_dir = DataDir(
        country_name=sys.argv[1], new_version_dir=sys.argv[2],
        old_version_root_dir=sys.argv[3],
    )
    mwm_diff_calculation(data_dir, logger, depth=1)
