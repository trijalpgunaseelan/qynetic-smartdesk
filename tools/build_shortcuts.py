#!/usr/bin/env python3
"""Create credential-free Scriptable shortcuts; macOS shortcuts CLI signs them."""
import pathlib,plistlib,uuid,subprocess,argparse
ROOT=pathlib.Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--sign',action='store_true');args=p.parse_args()
source=(ROOT/'main/web/iphone_sync.js').read_text()
for title,mode,filename in [('SmartDesk Auto Sync','both','smartdesk-sync.shortcut'),('SmartDesk Battery Sync','battery','smartdesk-battery.shortcut')]:
    script=source.replace("const mode = String(args.shortcutParameter || 'both').toLowerCase();",f"const mode = '{mode}';").replace("const automationRun = !!args.shortcutParameter && mode !== 'setup';","const automationRun = true;")
    assert f"const mode = '{mode}';" in script and "const automationRun = true;" in script
    workflow={
        'WFWorkflowName':title,'WFWorkflowClientVersion':'2600.0','WFWorkflowMinimumClientVersion':900,
        'WFWorkflowMinimumClientVersionString':'900','WFWorkflowIcon':{'WFWorkflowIconStartColor':1265074431,'WFWorkflowIconGlyphNumber':59511},
        'WFWorkflowImportQuestions':[],'WFWorkflowInputContentItemClasses':[],
        'WFWorkflowTypes':['NCWidget'],'WFQuickActionSurfaces':[],
        'WFWorkflowHasShortcutInputVariables':False,
        'WFWorkflowActions':[
            {'WFWorkflowActionIdentifier':'is.workflow.actions.delay','WFWorkflowActionParameters':{'WFDelayTime':2,'UUID':str(uuid.uuid4()).upper()}},
            {'WFWorkflowActionIdentifier':'dk.simonbs.Scriptable.RunScriptInlineIntent','WFWorkflowActionParameters':{
                'script':{'Value':{'string':script},'WFSerializationType':'WFTextTokenString'},
                'ShowWhenRun':False,'UUID':str(uuid.uuid4()).upper()
            }}
        ]}
    unsigned=ROOT/'shortcuts'/f'{title}.unsigned.shortcut'
    unsigned.write_bytes(plistlib.dumps(workflow,fmt=plistlib.FMT_BINARY,sort_keys=False))
    print('Prepared:',unsigned.relative_to(ROOT))
    if args.sign:
        output=ROOT/'main/web/shortcuts'/filename
        subprocess.run(['shortcuts','sign','--mode','anyone','--input',str(unsigned),'--output',str(output)],check=True)
        print('Signed:',output.relative_to(ROOT))
