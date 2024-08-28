import subprocess
import datetime

revision = (
    subprocess.check_output(["git", "log", "-1", "--pretty=format:'%h'"])
    # subprocess.check_output(["git", "rev-parse", "HEAD"])
    .strip()
    .decode("utf-8")
)

print(
    "'-DFIRMWARE_VERSION=\"{}-{}\"'".format(
        datetime.datetime.now().strftime("%Y%m%d"),
        revision
    )
)