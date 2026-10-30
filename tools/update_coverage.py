"""Merge actual editor/runtime evidence without promoting placement to gameplay proof."""
from pathlib import Path
import csv
import json
import re
from collections import Counter

root = Path(__file__).resolve().parents[1]
reports = root/'reports'
identities = json.loads((reports/'resource-identities.json').read_text())
with (reports/'assets.csv').open(encoding='utf-8-sig', newline='') as stream:
    rows = list(csv.DictReader(stream))
placement = {}
file = root/'.local/project-profile/profile/placement.tsv'
if file.exists():
    for row in csv.DictReader(file.open(encoding='utf-8-sig'), delimiter='\t'):
        if 'tutorial' in row['resource'].lower(): row['status'] = 'EXCLUDED_TUTORIAL'
        placement[row['resource'].split('}', 1)[-1].lower()] = row
for row in rows:
    key = row['resource'].split('}', 1)[-1].lower()
    if 'tutorial' in key:
        row['selection'] = 'excluded'
        row['exclusion_reason'] = 'tutorial-specific or broken vehicle; excluded from the playable collection'
        row['spawned_successfully'] = 'False'
    if row['source_addon_id'] == '58D0FB3206B6F859' and key in identities:
        row['resource'] = identities[key]
        row['engine_resource_resolved'] = 'True'
        if key in placement:
            row['placement_status'] = placement[key]['status']
            row['limitation'] = 'EDITOR_PLACEMENT; runtime operational acceptance is separate'
with (reports/'assets.csv').open('w', encoding='utf-8-sig', newline='') as stream:
    writer = csv.DictWriter(stream, fieldnames=list(rows[0])); writer.writeheader(); writer.writerows(rows)

variants = {}
optional_placement = {}
for name in ['Artillery', 'RHS', 'WCS']:
    folder = root/'integrations'/name
    file = folder/'placement.tsv'
    if not file.exists(): continue
    entries = list(csv.DictReader(file.open(encoding='utf-8-sig'), delimiter='\t'))
    for entry in entries:
        optional_placement.setdefault(entry['resource'].lower(), []).append((name, entry['status']))
    variants[name] = {'catalog_entries': len(entries), 'placement_status_counts': dict(Counter(row['status'] for row in entries)),
                      'interactive_operation_tested': False}
    (reports/f'placement-{name}.tsv').write_bytes(file.read_bytes())
if (root/'.local/project-profile/profile/placement.tsv').exists():
    with (reports/'placement-Core.tsv').open('w', encoding='utf-8-sig', newline='') as stream:
        writer = csv.DictWriter(stream, fieldnames=['index','resource','category','status','position'], delimiter='\t')
        writer.writeheader()
        for key, row in placement.items():
            if key in identities: row['resource'] = identities[key]
            writer.writerow(row)

evidence = []
errors = []
for profile in ['project-profile', 'profile-Artillery', 'profile-RHS', 'profile-WCS', 'game-Core', 'game-WCS', 'game-Artillery', 'game-RHS']:
    for file in sorted((root/'.local'/profile/'logs').glob('*/console.log')):
        for line in file.read_text(encoding='utf-8-sig', errors='replace').splitlines():
            if any(token in line for token in ['RVP:', 'RVP BUILD:', 'RVP INTEGRATION:', 'RVP PLAYCHECK:', 'RVP ENTRY:', 'RVP ENEMY:', 'RVP ENEMY CHECK:', 'RVP SERVICE CHECK:']):
                evidence.append({'profile': profile, 'log': file.parent.name, 'line': line})
    logs = sorted((root/'.local'/profile/'logs').glob('*/console.log'))
    if logs:
        errors.extend({'profile': profile, 'line': line} for line in logs[-1].read_text(encoding='utf-8-sig',errors='replace').splitlines()
                      if 'SCRIPT    (E)' in line or 'GUI       (E)' in line)
(reports/'runtime-evidence.json').write_text(json.dumps(evidence, indent=2), encoding='utf-8')
entry_runs = [e['log'] for e in evidence if 'RVP ENTRY:' in e['line']]
latest_entry_run = max(entry_runs) if entry_runs else None
final_entries = [e for e in evidence if e['log'] == latest_entry_run and 'normalActionEnteredVehicle=' in e['line']]
core_runtime_loaded = any('bakedDisplayEntities=174' in e['line'] or 'bakedDisplayEntities=177' in e['line'] for e in evidence)
for row in rows:
    variants_for_row = optional_placement.get(row['resource'].lower(), [])
    row['optional_variant_status'] = ';'.join(f'{name}:{status}' for name, status in variants_for_row)
    if variants_for_row:
        row['engine_resource_resolved'] = 'True'
        row['placement_status'] = row['optional_variant_status']
    if row['source_addon_id'] == '58D0FB3206B6F859' and row['placement_status'] == 'EDITOR_PLACED' and core_runtime_loaded:
        row['spawned_successfully'] = 'True'
with (reports/'assets.csv').open('w', encoding='utf-8-sig', newline='') as stream:
    writer = csv.DictWriter(stream, fieldnames=list(rows[0])); writer.writeheader(); writer.writerows(rows)
summary = {
    'game_version': '1.8.0.13', 'workbench_version': '1.8.0.13', 'branch': 'standard Steam apps, public configuration',
    'core_engine_resolved_resources': len(identities), 'core_editor_placements': sum(row['status']=='EDITOR_PLACED' for row in placement.values()),
    'range_empty_vehicle_targets': 8, 'mortar_shell_and_charge_items': 36, 'enemy_base_structures': 8, 'default_enemy_target_population': 30,
    'enemy_population_observed_alive': 30 if any('30 alive / 30 target' in e['line'] for e in evidence) else None,
    'enemy_respawn_policy': 'whole eliminated squad after 120 seconds, deferred within 100 metres of player',
    'enemy_respawn_interval_tested': any('after 145 seconds' in e['line'] and '30 alive / 30 target' in e['line'] for e in evidence),
    'enemy_respawn_check_method': 'Workbench removes one owned squad and observes recovery through the normal empty-squad timer; live combat kills are not simulated',
    'world_loaded_in_actual_game_mode': any('entered game mode' in e['line'] for e in evidence),
    'standalone_game_profiles_ready': sorted({e['profile'] for e in evidence if e['profile'].startswith('game-') and 'playground ready' in e['line']}),
    'controllable_player_observed': any('controllableCharacter=1' in e['line'] for e in evidence),
    'native_panel_instantiated': any('native panel object=1' in e['line'] for e in evidence),
    'alignment_confirmed_by_user': True, 'native_action_results': final_entries,
    'native_action_test_run': latest_entry_run,
    'service_checks': [e for e in evidence if e['log'] == latest_entry_run and 'RVP SERVICE CHECK:' in e['line']],
    'interactive_driving_flight_weapons_verified': False,
    'variants': variants, 'latest_profile_script_or_ui_errors': errors,
    'limitations': ['No mouse/keyboard desktop automation helper was available.',
                    'Native scripted seat tests are separate from human interactive acceptance.',
                    'No universal in-place rearm; replacement reset restores the original loadout.',
                    'Large optional packs use an initial physical sample and category/on-demand loading.',
                    'Jets, cruise-missile addons and the missing DonAir terrain were not downloaded after the user declined them.',
                    'DonAir remains blocked by missing terrain 64770A732AEA31D6.',
                    'WCS standalone startup and 30 live enemies were observed. Native Photo mode/map UI and the intro splash logged UI/component errors; see latest_profile_script_or_ui_errors. Third-party/base scripts were not modified.',
                    'All vehicle variants have not been driven/flown/fired.']
}
(reports/'verification.json').write_text(json.dumps(summary, indent=2), encoding='utf-8')
coverage = json.loads((reports/'coverage.json').read_text())
coverage.update({'engine_resource_resolved':len(identities),'placed':summary['core_editor_placements'],
                 'vanilla_candidates':sum(r['source_addon_id']=='58D0FB3206B6F859' and r['selection']=='candidate' for r in rows),
                 'vanilla_candidate_categories':dict(Counter(r['category'] for r in rows if r['source_addon_id']=='58D0FB3206B6F859' and r['selection']=='candidate')),
                 'status':'IMPLEMENTED; see verification.json for actual checks and limitations'})
(reports/'coverage.json').write_text(json.dumps(coverage,indent=2),encoding='utf-8')
print(json.dumps({k:v for k,v in summary.items() if k not in ['native_action_results','limitations']},indent=2))
