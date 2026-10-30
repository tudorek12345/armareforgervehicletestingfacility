"""Inventory local content without mounting mods, changing presets, or writing game files."""
from __future__ import annotations
import csv
import json
import os
import re
import sys
from collections import Counter
from functools import lru_cache
from pathlib import Path
from read_installed import Pak

ROOT = Path(__file__).resolve().parents[1]
REF = re.compile(r'\{([0-9A-Fa-f]{16})\}([^"\r\n]+)')
PARENT = re.compile(r'^\s*(\w+)\s*(?::\s*"(?:\{[0-9A-Fa-f]{16}\})?([^"\r\n]+)")?\s*\{')


def discover_steam():
    import winreg
    with winreg.OpenKey(winreg.HKEY_CURRENT_USER, r'Software\Valve\Steam') as key:
        steam = Path(winreg.QueryValueEx(key, 'SteamPath')[0])
    libraries = {steam}
    for path in re.findall(r'"path"\s*"([^"]+)"', (steam/'steamapps/libraryfolders.vdf').read_text()):
        libraries.add(Path(path.replace('\\\\', '\\')))
    apps = {}
    for library in libraries:
        for appid in ['1874880', '1874910', '1890860', '1890880']:
            manifest = library/f'steamapps/appmanifest_{appid}.acf'
            if manifest.exists():
                data = dict(re.findall(r'"([^"\r\n]+)"\s*"([^"\r\n]*)"', manifest.read_text()))
                apps[appid] = {'path': str(library/'steamapps/common'/data['installdir']),
                               'build_id': data.get('buildid'), 'beta': data.get('BetaKey', 'public'),
                               'manifest': str(manifest), 'state_flags': data.get('StateFlags')}
    return steam, sorted(map(str, libraries)), apps


def main():
    local = ROOT/'.local'; local.mkdir(exist_ok=True)
    reports = ROOT/'reports'; reports.mkdir(exist_ok=True)
    steam, libraries, apps = discover_steam()
    if '1874880' not in apps:
        raise SystemExit('Standard game not found; no files changed in the game installation.')
    game = Path(apps['1874880']['path'])
    user = Path.home()
    addon_roots = [user/'Documents/My Games/ArmaReforger/addons',
                   user/'OneDrive/Documents/My Games/ArmaReforger/addons',
                   user/'Documents/My Games/ArmaReforger/profile/addons',
                   user/'OneDrive/Documents/My Games/ArmaReforger/profile/addons']
    projects = list((game/'addons').glob('*/*.gproj'))
    for folder in addon_roots:
        if folder.exists(): projects.extend(folder.glob('*/*.gproj'))
    addons, by_guid, sources, errors = [], {}, {}, []
    for project in sorted(set(projects)):
        data = project.read_text(encoding='utf-8-sig', errors='replace')
        def field(key):
            m = re.search(r'\b'+key+r'\s+"([^"]*)"', data)
            return m[1] if m else ''
        dep = re.search(r'\bDependencies\s*\{([^}]*)}', data)
        deps = re.findall(r'"([A-Fa-f0-9]{16})"', dep[1]) if dep else []
        meta = {}
        if (project.parent/'ServerData.json').exists():
            try: meta = json.loads((project.parent/'ServerData.json').read_text(encoding='utf-8-sig'))
            except (ValueError, OSError): pass
        revision = meta.get('revision') or {}
        addon = {'id': field('GUID'), 'name': field('TITLE') or field('ID'),
                 'project_id': field('ID'), 'dependencies': deps,
                 'version': revision.get('version', ''), 'declared_game_version': revision.get('gameVersion', ''),
                 'corrupted': revision.get('corrupted'), 'path': str(project.parent),
                 'compatibility': 'NOT_COMPILED_OR_MOUNTED'}
        addons.append(addon); by_guid[addon['id']] = addon
        for packed in sorted(project.parent.glob('*.pak')):
            if 'shader' in packed.name: continue
            try:
                archive = Pak(packed)
                for name in archive.entries:
                    if name.lower().endswith(('.et', '.conf', '.ent', '.layer')):
                        sources[(addon['id'], name.lower())] = (archive, name)
            except (ValueError, OSError, UnicodeError) as exc:
                errors.append({'addon': addon['id'], 'archive': packed.name, 'error': str(exc)})
    def closure(guid, visited=None):
        visited = set() if visited is None else visited
        if guid in visited: return visited, set()
        visited.add(guid); missing = set()
        for dep in by_guid[guid]['dependencies']:
            if dep not in by_guid: missing.add(dep)
            else:
                _, absent = closure(dep, visited); missing.update(absent)
        return visited, missing
    for addon in addons:
        deps, missing = closure(addon['id'])
        addon['dependency_closure'] = sorted(deps - {addon['id']})
        addon['missing_dependencies'] = sorted(missing)
        if missing: addon['compatibility'] = 'BLOCKED_MISSING_DEPENDENCIES'
        if addon['corrupted']: addon['compatibility'] = 'BLOCKED_CORRUPTED_DOWNLOAD'

    @lru_cache(maxsize=12000)
    def read(guid, path):
        item = sources.get((guid, path.lower()))
        if not item: return ''
        archive, name = item
        try: return archive.read(name).decode('utf-8-sig')
        except (ValueError, UnicodeError, OSError): return ''

    base_id = '58D0FB3206B6F859'
    def resolve(guid, path):
        if (guid, path.lower()) in sources: return guid, path.lower()
        for dep in by_guid[guid]['dependency_closure']:
            if (dep, path.lower()) in sources: return dep, path.lower()
        return None

    catalog_refs, identities = {}, {}
    # Catalogs first. Entries disabled in source stay excluded. Runtime performs the authoritative merge.
    for (guid, lower), (archive, name) in sources.items():
        if 'entitycatalog' not in lower or not lower.endswith('.conf'): continue
        if not any(x in lower for x in ('vehicle', 'weapontripod', 'static', 'artillery')): continue
        data = read(guid, lower)
        for match in re.finditer(r'm_sEntityPrefab\s+"(?:\{([A-Fa-f0-9]{16})\})?([^"\r\n]+)"', data):
            identity, prefab = match.groups(); resolved = resolve(guid, prefab)
            if resolved:
                catalog_refs.setdefault(resolved, []).append(f'{guid}:{name}')
                if identity: identities[resolved] = identity.upper()

    @lru_cache(maxsize=12000)
    def chain(guid, lower, depth=0):
        if depth > 24: return '', '', 'inheritance cycle/depth limit'
        text = read(guid, lower); match = PARENT.match(text)
        if not match: return '', text, 'unreadable/non-text prefab'
        cls, parent = match.groups()
        if parent:
            target = resolve(guid, parent)
            if not target: return cls, text, f'unresolved parent: {parent}'
            root_cls, parent_text, issue = chain(*target, depth+1)
            return root_cls or cls, text+'\n'+parent_text, issue
        return cls, text, ''

    rows = []
    for key, (archive, path) in sorted(sources.items()):
        guid, lower = key
        if not lower.endswith('.et'): continue
        catalog = key in catalog_refs
        if not catalog and not any(x in lower for x in ('/vehicles/', '/weapons/static/', '/weapons/mortars/', '/weapons/artillery/')): continue
        cls, text, issue = chain(guid, lower)
        reason = ''
        if 'tutorial' in lower:
            reason = 'tutorial-specific or broken vehicle; excluded from the playable collection'
        elif any(x in lower for x in ('/vehparts/', '/wreck', '/destruction/', '/debris/', '/projectiles/', '/turrets/', '/weaponparts/', '/dst/', '_gun_mount', 'cinematic_')):
            reason = 'part, wreck, projectile or embedded turret prefab'
        elif not catalog and re.search(r'(^|[/_])(base|abstract|proxy|slot|test)([/_.]|$)', lower):
            reason = 'uncataloged parent/template/test prefab'
        elif cls not in ('Vehicle', 'Turret') and not any(x in text for x in ('TurretControllerComponent', 'SCR_MortarComponent')):
            reason = f'no vehicle/crew-weapon root confirmed ({cls or "unknown"})'
        elif not catalog and 'Compartment' not in text:
            reason = 'no crew compartment confirmed in inherited text'
        elif issue:
            reason = issue
        if any(x in lower for x in ('helicopter', '/mi8', '/uh1')): category = 'Helicopters'
        elif any(x in text for x in ('AirplaneControllerComponent', 'PlaneControllerComponent')): category = 'FixedWing'
        elif any(x in lower for x in ('/boats/', '/water/', '/watercraft/')): category = 'Boats'
        elif any(x in lower for x in ('/tracked/', '/armored/', '/btr', '/lav', '/brdm', '/tank')): category = 'Armor'
        elif any(x in lower for x in ('/weapons/', '/static/', '/artillery/')) or cls == 'Turret': category = 'Weapons'
        else: category = 'Ground'
        names = re.findall(r'\bName\s+"([^"]+)"', text)
        display = names[0] if names else Path(path).stem
        rows.append({'display_name': display, 'display_name_status': 'localization token' if display.startswith('#') else 'source/fallback name',
                     'resource': ('{'+identities[key]+'}' if key in identities else '')+path,
                     'category': category, 'source_addon': by_guid[guid]['name'], 'source_addon_id': guid,
                     'dependencies': ';'.join(by_guid[guid]['dependency_closure']),
                     'catalog_sources': ';'.join(catalog_refs.get(key, [])),
                     'selection': 'excluded' if reason else 'candidate', 'exclusion_reason': reason,
                     'discovered': True, 'archive_entry_read': bool(text), 'engine_resource_resolved': False,
                     'spawned_successfully': False, 'interactively_tested': False,
                     'placement_status': 'NOT_PLACED',
                     'limitation': 'FUNCTIONALITY_UNVERIFIED; '+by_guid[guid]['compatibility']})
    verified_file = reports/'resource-identities.json'
    verified = json.loads(verified_file.read_text(encoding='utf-8')) if verified_file.exists() else {}
    for row in rows:
        path_key = row['resource'].split('}', 1)[-1].lower()
        if row['source_addon_id'] == base_id and path_key in verified:
            row['resource'] = verified[path_key]
            row['engine_resource_resolved'] = True
    with (reports/'assets.csv').open('w', newline='', encoding='utf-8-sig') as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0])); writer.writeheader(); writer.writerows(rows)
    public_addons = [{k:v for k,v in a.items() if k != 'path'} for a in addons]
    (reports/'addons.json').write_text(json.dumps(public_addons, indent=2), encoding='utf-8')
    (local/'installation.json').write_text(json.dumps({'steam':str(steam),'libraries':libraries,'apps':apps,
       'game_executable':str(game/'ArmaReforgerSteam.exe'),'game_diag':str(game/'ArmaReforgerSteamDiag.exe'),
       'addon_roots':list(map(str,addon_roots)),'project_root':str(ROOT/'addon'),
       'reference_cache':str(user/'AppData/Local/ReforgerVehiclePlayground/references/installed')}, indent=2),encoding='utf-8')
    core = [r for r in rows if r['source_addon_id'] == base_id and r['selection']=='candidate']
    # Fallback list references only the game's own prefabs; optional resources cannot break core loading.
    code = ['// Generated from installed archives. Engine/runtime validation is still required.',
            'class RVP_InstalledCandidates', '{', ' static void Get(out array<ResourceName> resources)', ' {', '  resources = {};']
    code += ['  resources.Insert("'+r['resource']+'");' for r in core]
    code += [' }', '}']
    dest=ROOT/'addon/Scripts/Game/RVP'; dest.mkdir(parents=True,exist_ok=True)
    # Existing world entity names use stable slot indexes. A fresh inventory must not renumber those slots.
    registry = dest/'RVP_InstalledCandidates.c'
    if not registry.exists(): registry.write_text('\n'.join(code)+'\n',encoding='utf-8')
    (reports/'discovered-core-resources.txt').write_text('\n'.join(r['resource'] for r in core)+'\n',encoding='utf-8')
    summary={'installed_addons_including_core':len(addons),'third_party_addon_directories':len(addons)-2,
             'inspected_candidates_and_exclusions':len(rows),'vanilla_candidates':len(core),
             'vanilla_candidate_categories':dict(Counter(r['category'] for r in core)),
             'optional_candidates':sum(r['selection']=='candidate' and r['source_addon_id']!=base_id for r in rows),
             'engine_resource_resolved':len(verified),'placed':0,'interactively_tested':0,'archive_errors':errors,
             'workbench_installed':'1874910' in apps,
             'status':'INVENTORY_REFRESHED; run tools/update_coverage.py to merge recorded editor/runtime evidence'}
    (reports/'coverage.json').write_text(json.dumps(summary,indent=2),encoding='utf-8')
    print(json.dumps(summary,indent=2))

if __name__ == '__main__': main()
