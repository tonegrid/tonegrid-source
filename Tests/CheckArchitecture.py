"""Resolved application boundaries. Callback tokens are a heuristic, not proof."""
from pathlib import Path
import re
ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'Source'
checks = []
def check(condition, label):
    checks.append((bool(condition), label))
    print(('PASS ' if condition else 'FAIL ') + label)
def feature(path):
    parts = path.relative_to(SOURCE).parts
    return parts[1] if len(parts) > 2 and parts[0] == 'Modules' else None
def reachable(graph, start):
    visited = set()
    pending = list(graph.get(start, ()))
    while pending:
        item = pending.pop()
        if item in visited: continue
        visited.add(item)
        pending.extend(graph.get(item, ()))
    return visited
files = [p for p in SOURCE.rglob('*') if p.suffix in ('.h', '.cpp', '.mm', '.inl')]
texts = {p: p.read_text(encoding='utf-8-sig') for p in files}
graph, missing = {}, []
for path, text in texts.items():
    graph[path] = set()
    for name in re.findall(r'^\s*#\s*include\s*"([^"]+)"', text, re.M):
        choices = [path.parent / name, SOURCE / name, ROOT / name]
        target = next((p.resolve() for p in choices if p.exists()), None)
        if target is None: missing.append((path.relative_to(ROOT).as_posix(), name))
        elif target.is_relative_to(SOURCE): graph[path].add(target)
check(not missing, 'All quoted source includes resolve' + (': '+str(missing) if missing else ''))
for name in ('Play','Jam','Learn','Record'):
    bad=[(p.relative_to(SOURCE).as_posix(),q.relative_to(SOURCE).as_posix()) for p in files if feature(p)==name for q in reachable(graph,p) if feature(q) not in (None,name)]
    check(not bad,name+' has no direct or transitive feature dependency'+(': '+str(bad[:5]) if bad else ''))
for area in ('Core','Effects','Content','Shared'):
    bad=[(p.relative_to(SOURCE).as_posix(),q.relative_to(SOURCE).as_posix()) for p in files if p.is_relative_to(SOURCE/area) for q in reachable(graph,p) if feature(q)]
    check(not bad,area+' has no direct or transitive feature dependency'+(': '+str(bad[:5]) if bad else ''))
a,b,c=SOURCE/'Modules/Learn/Fake.h',SOURCE/'Shared/Fake.h',SOURCE/'Modules/Jam/Fake.h'
check(c in reachable({a:{b},b:{c}},a),'Traversal detects an indirect feature dependency fixture')
cmake=(ROOT/'CMakeLists.txt').read_text(encoding='utf-8-sig')
check('add_library(tonegrid_core STATIC' in cmake and 'add_library(tonegrid_record STATIC' in cmake,'Core and Record are compiled targets')
check('foreach(module Play Jam Learn)' in cmake and 'add_library(tonegrid_${tab} STATIC' in cmake,'Play/Jam/Learn are compiled targets rather than interface wrappers')
check('target_link_libraries(tonegrid_record PUBLIC tonegrid_core' in cmake,'Record depends on Core')
check(not re.search(r'target_link_libraries\(tonegrid_core[^)]*tonegrid_(record|play|jam|learn)',cmake,re.S),'Core does not link a feature')
check(not (SOURCE/'Core/IToneGridModule.h').exists() and (SOURCE/'Contracts/Module.h').exists(),'Module view contract is outside audio Core')
engine=(SOURCE/'Core/RecordAudioEngine.h').read_text(encoding='utf-8')
callback=engine[engine.index(' const float* const* input('):engine.index(' static constexpr int capacity')]
check(all(t not in callback for t in ('createOutputStream','createInputStream','replaceWithText','std::mutex','ScopedLock','JSON::','XmlDocument','AudioThumbnail','evaluateJavascript','new ')),'Recorder callback heuristic finds no direct IO/lock/GUI/serialization/allocation')
check('writer->write(&source,frames)' in callback and 'ThreadedWriter' in engine,'Recording uses bounded threaded-writer handoff')
check('buildGuitarGraph(graph,std::move(pendingBoard)' in (SOURCE/'App/BoardMain.cpp').read_text(encoding='utf-8') and 'buildGuitarGraph(out->graph' in (SOURCE/'Core/ProcessingGraph.cpp').read_text(encoding='utf-8'),'Live/render share graph construction')
check('captures::resolveKind' in (SOURCE/'Core/ProcessingGraph.cpp').read_text(encoding='utf-8') and 'captures::resolveKind' in (SOURCE/'App/CabinetHost.inl').read_text(encoding='utf-8'),'Live fallback/render share capture classification')
print(f'{len(checks)} architecture/source checks; '+('ALL PASSED' if all(x[0] for x in checks) else 'FAILURES'))
raise SystemExit(0 if all(x[0] for x in checks) else 1)
