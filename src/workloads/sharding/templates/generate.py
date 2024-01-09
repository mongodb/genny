import subprocess
import os
import itertools
from pathlib import Path
import shutil

def copyFileWithHeader(inFilename, outFilename):
    header = "# WARNING: This is a generated file. Do not modify. See templates directory for more information.\n"
    with open(inFilename, "r") as inFile, open(outFilename, "w") as outFile:
        outFile.write(header + inFile.read())

def getScriptDir():
    return Path(__file__).parent.absolute()

def getOutDir():
    return Path(getScriptDir()).parent.absolute()

def getWorkloadFilename(base, enabledPatches):
    templateName = base.parent.name
    name = getOutDir().joinpath(templateName, base.stem)
    patchStems = [patch.stem for patch in enabledPatches]
    patchStems.sort()
    for patchStem in patchStems:
        name = name.parent.joinpath(f"{name.stem}-{patchStem}.yml")
    return name

def enablePatch(outFile, patch):
    subprocess.run(["patch", "--no-backup-if-mismatch", outFile, patch])

def generateTemplateWorkload(base, enabledPatches):
    out = getWorkloadFilename(base, enabledPatches)
    out.parent.mkdir(parents=True, exist_ok=True)
    copyFileWithHeader(base, out)
    for patch in enabledPatches:
        enablePatch(out, patch)

def generateAllTemplateWorkloads(base, patches):
    for n in range(len(patches) + 1):
        for combination in itertools.combinations(patches, n):
            generateTemplateWorkload(base, combination)

def generateTemplate(templateDir):
    base = list(templateDir.glob("*.yml.base"))
    if len(base) != 1:
        raise RuntimeError(f"Template directory {templateDir} contains {len(base)} base files; it must contain one and only one base file with extension .yml.base")
    patches = list(templateDir.glob("*.patch"))
    generateAllTemplateWorkloads(base[0], patches)

def generateAllTemplates():
    for file in os.scandir(getScriptDir()):
        if file.is_dir():
            generateTemplate(Path(file.path))

def main():
    generateAllTemplates()

main()
