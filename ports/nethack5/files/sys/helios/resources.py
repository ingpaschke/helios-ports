# Prepare X resources with the host preprocessor, leaving the upstream file readable.
from pathlib import Path
import subprocess
text=subprocess.check_output(['cc','-E','-P','-x','c','win/X11/NetHack.ad'],text=True)
Path('NetHack5').write_text(text)
