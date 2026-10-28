"""POL-002 ordered historical/candidate preparation and nested six-shard gate.

Historical test code and publication authority are exported from an immutable
Git commit. Candidate policies cannot authorize production in the same delta.
Every outer report binds the exact event head/tree, epochs, sealed input bundle,
run/attempt and complete inner historical execution. No PR can deploy.
"""

import argparse
from collections import Counter
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tarfile
import tempfile

import documentation_ci as ci
import feature_source as source
import source_epoch as epoch

ROOT = Path(__file__).resolve().parents[2]
HISTORY = source.HISTORICAL_MAIN
HISTORY_TREE = source.HISTORICAL_TREE
POLICY_PATHS = frozenset({
    '.github/workflows/documentation.yml',
    'docs/tools/ada_examples.py', 'docs/tools/local_type_audit.py',
    'docs/tools/feature_source.py', 'docs/tools/source_epoch.py',
    'docs/tools/qualify_feature_source.py', 'docs/tools/epoch_ci.py',
    'docs/tests/pol002_source_tests.py', 'docs/tests/pol002_epoch_tests.py',
})
PROTECTED_INPUTS = (
    'scripts/check_doc_source_integrity.py', 'scripts/check_doc_coverage.py',
    'scripts/gnatdoc_coverage_model.py', 'scripts/compare_gnatdoc_warnings.py',
    'docs/gnatdoc-native-anchors.json', 'docs/gnatdoc-renderer-exceptions.json',
    'docs/gnatdoc-coverage-baseline.txt', 'docs/gnatdoc-warning-baseline.txt',
    'docs/tools/documentation_ci.py', 'docs/tools/build_enhanced_site.py',
    'docs/tests/ci001-inherited-inventory.json',
)
require = source.require

# Exact CI-ACTIONS-001 dependency proposal only. Mechanical policy qualification
# cannot grant source/publication approval or authorize unrelated production.
ACTIONS_SOURCE = (
    ('.github/workflows/ci.yml',
     'c89a50fab57f9bbb7653cf26a8b0260f72d9436c1ebde188dde4b29f257514f2',
     'de38dd1e155bd1cf7d256995f6c8bded61e2fe000f1e733d3dad8a8f255fe235'),
    ('.github/workflows/documentation.yml',
     '979931734ae82a2a98b8684231f06ce17f202c4115eafdf439136c238724a847',
     'cb7ea4aa28d20bec2976af8d430fbff84d00bdc69f19b1dd13885f41022cc7d3'),
    ('scripts/check_doc_source_integrity.py',
     'cf7d8bf88288bdbe76be4d8fefa9970d2be2cdd9adfaf296ebbd682d63bd9d7d',
     'e60abd114b984ee3d1f1ce6905c74045e3f1d8ee75da0ba2ac1141f40c726ef9'),
    ('docs/tools/test_actions_dependencies.py', None,
     '4312b37b2d54056e83b2970ed84ec1d50e96d4ca4c41a2c273d2fd66b3d5eabc'),
)


def actions_delta(root, target, candidate, old, new):
    special = {'.github/workflows/ci.yml', 'scripts/check_doc_source_integrity.py',
               'docs/tools/test_actions_dependencies.py'}
    if not any(old.get(n) != new.get(n) for n in special):
        return set()
    for name, predecessor, proposed in ACTIONS_SOURCE:
        require(new.get(name, (None,))[0] == '100644', 'CI-ACTIONS-001 candidate mode/missing: ' + name)
        require(hashlib.sha256(source.blob(root, candidate, name)).hexdigest() == proposed,
                'CI-ACTIONS-001 candidate bytes: ' + name)
        if predecessor is None:
            require(name not in old, 'CI-ACTIONS-001 unexpected predecessor: ' + name)
        else:
            require(old.get(name, (None,))[0] == '100644' and
                    hashlib.sha256(source.blob(root, target, name)).hexdigest() == predecessor,
                    'CI-ACTIONS-001 predecessor bytes/mode: ' + name)
    return {name for name, _, _ in ACTIONS_SOURCE}


def digest(value):
    return hashlib.sha256(epoch.canonical(value)).hexdigest()


def write(path, value):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(epoch.canonical(value))


def git(*args):
    return source.git(ROOT, *args)


def context():
    """GitHub's read-only event payload, never PR-supplied labels/manifests."""
    event = ci.load(os.environ['GITHUB_EVENT_PATH'])
    kind = os.environ['GITHUB_EVENT_NAME']
    repository = os.environ['GITHUB_REPOSITORY']
    require(event['repository']['full_name'] == repository, 'event repository mismatch')
    if kind == 'pull_request':
        pr = event['pull_request']
        require(pr['base']['ref'] == 'main' and pr['base']['repo']['full_name'] == repository,
                'PR must target trusted main repository')
        target, candidate = pr['base']['sha'], pr['head']['sha']
        ref = 'refs/remotes/origin/pull/' + str(event['number']) + '/head'
        if subprocess.run(['git', '-C', str(ROOT), 'rev-parse', '--verify', ref],
                          capture_output=True).returncode != 0:
            subprocess.run(['git', '-C', str(ROOT), 'fetch', 'origin',
                            'refs/pull/' + str(event['number']) + '/head:' + ref], check=True)
        require(source.revision(ROOT, ref) == candidate, 'event head differs from fetched PR ref')
        require(os.environ['GITHUB_REF'] == 'refs/pull/' + str(event['number']) + '/merge', 'wrong PR event ref')
    else:
        require(kind == 'push' and event['ref'] == 'refs/heads/main'
                and os.environ['GITHUB_REF'] == 'refs/heads/main', 'unsupported source event')
        candidate, target = event['after'], event['before']
        require(candidate == os.environ['GITHUB_SHA'], 'push/event SHA mismatch')
    require(all(re.fullmatch(r'[0-9a-f]{40}', x) for x in (target, candidate)), 'malformed event commit')
    require(source.revision(ROOT, 'HEAD') == candidate, 'event candidate is not checkout HEAD')
    require(source.revision(ROOT, HISTORY) == HISTORY and
            git('rev-parse', HISTORY + '^{tree}').decode().strip() == HISTORY_TREE,
            'historical source authority mismatch')
    for ancestor, descendant in ((HISTORY, target), (target, candidate),
                                 (target, source.revision(ROOT, 'refs/remotes/origin/main'))):
        require(subprocess.run(['git', '-C', str(ROOT), 'merge-base', '--is-ancestor', ancestor, descendant],
                               capture_output=True).returncode == 0, 'unqualified source ancestry')
    source.exact_checkout(ROOT, candidate, source.tree(ROOT, candidate))
    result = {'source_sha': candidate, 'source_tree': git('rev-parse', candidate + '^{tree}').decode().strip(),
              'target_sha': target, 'target_tree': git('rev-parse', target + '^{tree}').decode().strip(),
              'historical_sha': HISTORY, 'historical_tree': HISTORY_TREE,
              'event': kind, 'repository': repository, 'run_id': os.environ['GITHUB_RUN_ID'],
              'run_attempt': os.environ['GITHUB_RUN_ATTEMPT'], 'qualification_mode': 'hosted'}
    if os.environ.get('DOC_LOCAL_QUALIFICATION') == '1':
        require(os.environ.get('GITHUB_ACTIONS') != 'true', 'local qualification forbidden inside hosted actions')
        result['qualification_mode'] = 'local-not-deployable'
    require(result['run_id'].isdigit() and result['run_attempt'].isdigit(), 'invalid run identity')
    return result


def classify(root, target, candidate):
    """Policy proposals may evaluate policy, but cannot mix production changes.

    Feature deltas must carry all authorization tools byte-identical to trusted
    target main. A policy-only candidate is explicitly not independent approval.
    """
    old, new = source.tree(root, target), source.tree(root, candidate)
    require(subprocess.run(['git', '-C', str(root), 'merge-base', '--is-ancestor', target, candidate],
                           capture_output=True).returncode == 0, 'unqualified policy/source ancestry')
    changed = sorted(n for n in old.keys() | new.keys() if old.get(n) != new.get(n))
    policy = any(n in POLICY_PATHS for n in changed)
    if not policy:
        value = source.qualify(root, target, candidate)
        for name in POLICY_PATHS:
            require(old.get(name) == new.get(name), 'feature changed qualification implementation')
        return value
    actions = actions_delta(root, target, candidate, old, new)
    for name in changed:
        require(name in new and new[name][0] == old.get(name, ('100644',))[0], 'policy mode/removal change')
        require(name in POLICY_PATHS or name in actions or source.documentation_path(name),
                'policy mixed with protected production/source change: ' + name)
    for name in PROTECTED_INPUTS:
        require(name in actions or old.get(name) == new.get(name),
                'policy proposal changed inherited authority: ' + name)
    source.exact_checkout(root, source.revision(root, candidate), new)
    return {'schema': 1, 'case': 'policy-only', 'target_commit': source.revision(root, target),
            'target_tree': source.git(root, 'rev-parse', target + '^{tree}').decode().strip(),
            'candidate_commit': source.revision(root, candidate),
            'candidate_tree': source.git(root, 'rev-parse', candidate + '^{tree}').decode().strip(),
            'delta': [{'path': n, 'old': list(old[n]) if n in old else None, 'new': list(new[n])} for n in changed],
            'independent_approval': False, 'publication_epoch_approved': False}


def authority(root, value):
    """Feature execution uses only target tools; policy evaluation is a separate case."""
    if value['case'] != 'policy-only':
        for name in POLICY_PATHS | set(source.POLICY_FILES):
            path = Path(root) / name
            if path.exists():
                require(path.read_bytes() == source.blob(root, value['target_commit'], name),
                        'candidate qualifier differs from trusted target: ' + name)


def execute(command, cwd, log, env=None):
    with Path(log).open('wb') as stream:
        subprocess.run(command, cwd=cwd, env=env, stdin=subprocess.DEVNULL,
                       stdout=stream, stderr=subprocess.STDOUT, check=True)


def qualified_alire_environment(settings, log, base=None):
    """Select the pinned toolchain in task-only settings for *nested* alr calls.

    The outer `alr -s ... exec` does not make -s transitive. ALIRE_SETTINGS_DIR
    does. Preselect noninteractively before the unchanged formatter script runs;
    EOF stdin catches accidental prompts rather than answering them blindly.
    """
    settings = Path(settings).absolute()
    settings.mkdir(parents=True, exist_ok=True)
    require(not any(p.is_symlink() for p in (settings, *settings.parents)), 'symlink Alire settings')
    env = dict(os.environ if base is None else base)
    # Reuse only an explicitly supplied task cache/tool install, never infer
    # selection from HOME. Fresh fixture settings retain independent defaults.
    inherited = env.get('ALIRE_SETTINGS_DIR')
    if inherited and Path(inherited).absolute() != settings:
        inherited = Path(inherited).absolute()
        require(not any(p.is_symlink() for p in (inherited, *inherited.parents)), 'symlink inherited Alire settings')
        for key, path in (('cache.dir', inherited / 'cache'),
                          ('toolchain.dir', inherited / 'cache/toolchains')):
            execute(['alr', '-n', '-s', str(settings), 'settings', '--global', '--set', key, str(path)],
                    settings, Path(log).with_name(Path(log).stem + '-' + key + '.log'),
                    {**env, 'ALIRE_SETTINGS_DIR': str(settings)})
    env['ALIRE_SETTINGS_DIR'] = str(settings)
    env['DOC_EXAMPLES_ALIRE_SETTINGS'] = str(settings)
    execute(['alr', '-n', '-s', str(settings), '--force', 'toolchain', '--select',
             'gnat_native=16.1.0', 'gprbuild=26.0.1', '--disable-assistant'], settings, log, env)
    for name, expected in (('gnat', 'gnat_native=16.1.0'), ('gprbuild', 'gprbuild=26.0.1')):
        actual = subprocess.check_output(['alr', '-n', '-s', str(settings), 'settings', '--global',
                                          '--get', 'toolchain.use.' + name], cwd=settings, env=env,
                                         stdin=subprocess.DEVNULL, text=True).strip()
        require(actual == expected, 'unqualified nested Alire selection: ' + name)
    return env


def historical_environment(outer):
    env = dict(os.environ)
    env.update(DOC_SOURCE_SHA=HISTORY, GITHUB_RUN_ID=outer['run_id'],
               GITHUB_RUN_ATTEMPT=outer['run_attempt'], GITHUB_REPOSITORY=outer['repository'])
    return env


def historical_checkout(path):
    """Separate Git clone: no source writer shares a historical fixture tree."""
    subprocess.run(['git', 'clone', '--shared', '--no-checkout', str(ROOT), str(path)],
                   check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    subprocess.run(['git', '-C', str(path), 'checkout', '--detach', HISTORY], check=True,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def file_map(path):
    import build_enhanced_site as p
    return p.hashes(p.files(path))


def validate_policy(report, expected):
    require(report['successful'] is True and report['tests'] == expected, 'policy inventory/result mismatch')
    require(expected and len(expected) == len(set(expected)), 'empty/duplicate policy inventory')
    events = report['events']
    require(all(e['event'] in {'start', 'success', 'stop'} for e in events), 'policy non-success event')
    for kind in ('start', 'success', 'stop'):
        require(Counter(e['id'] for e in events if e['event'] == kind) == Counter(expected),
                'missing/duplicate policy execution')
    require(len(events) == 3 * len(expected), 'extra policy callbacks')
    for offset in range(0, len(events), 3):
        triplet = events[offset:offset + 3]
        require([e['event'] for e in triplet] == ['start', 'success', 'stop'] and
                len({e['id'] for e in triplet}) == 1, 'invalid/overlapping policy event sequence')
        require(type(triplet[-1]['seconds']) in (int, float) and triplet[-1]['seconds'] >= 0,
                'invalid policy execution timing')


def prepare(directory, prefix):
    """All writers complete before seal. No test can consume partial fixtures."""
    outer = context()
    value = classify(ROOT, outer['target_sha'], outer['source_sha'])
    authority(ROOT, value)
    if value['case'] != 'policy-only':
        require(source.blob(ROOT, value['target_commit'], 'docs/tools/epoch_ci.py') == Path(__file__).read_bytes(),
                'feature orchestration must execute trusted target bytes')
    directory = Path(directory).absolute()
    require(not directory.exists(), 'epoch preparation directory must be fresh')
    directory.mkdir(parents=True)
    history = directory / 'historical'
    historical_checkout(history)
    logs = directory / 'logs'
    logs.mkdir()
    env = historical_environment(outer)
    env['GNATDOC_PREFIX'] = str(Path(prefix).absolute())
    env = qualified_alire_environment(ROOT / '.doc-tools/settings', logs / 'alire-selection.log', env)
    py = sys.executable
    execute(['python3', 'scripts/check_doc_source_integrity.py'], history, logs / 'historical-source.log', env)
    execute(['sh', 'scripts/generate_docs_baseline.sh'], history, logs / 'historical-generation.log', env)
    execute([py, 'docs/tools/build_enhanced_site.py', 'build'], history, logs / 'historical-enhancement.log', env)
    execute([py, 'docs/tools/build_enhanced_site.py', 'verify'], history, logs / 'historical-verification.log', env)
    # Generate/verify second historical copy in a different source clone, not a
    # rewrite of the tree that consumers will use.
    with tempfile.TemporaryDirectory(prefix='pol002-historical-second-') as temp:
        second = Path(temp) / 'source'
        historical_checkout(second)
        execute(['sh', 'scripts/generate_docs_baseline.sh'], second, logs / 'historical-generation2.log', env)
        execute([py, 'docs/tools/build_enhanced_site.py', 'build'], second, logs / 'historical-enhancement2.log', env)
        require(file_map(history / '_site') == file_map(second / '_site') and
                file_map(history / '_site-published') == file_map(second / '_site-published'),
                'historical regeneration not deterministic')
    execute([py, 'docs/tools/documentation_ci.py', 'inventory'], history, logs / 'inventory.log', env)
    execute([py, 'docs/tools/documentation_ci.py', 'run', '--owner', 'scripts',
             '--output', 'qualification/scripts.json'], history, logs / 'scripts.log', env)
    execute([py, 'docs/tools/documentation_ci.py', 'seal', '--output', str(directory / 'historical.tar')],
            history, logs / 'historical-seal.json', env)
    seal = ci.load(logs / 'historical-seal.json')
    # Policy tests generate their own committed synthetic repositories. Their
    # native outputs never share history's _site, _site-published or obj/docs.
    execute([py, 'docs/tests/pol002_source_tests.py', '-v', '--report', str(directory / 'policy.json')],
            ROOT, logs / 'policy.log', env)
    policy_report = ci.load(directory / 'policy.json')
    validate_policy(policy_report, policy_report['tests'])
    execute([py, 'docs/tests/pol002_epoch_tests.py', '-v', '--report', str(directory / 'integration.json')],
            ROOT, logs / 'integration.log', env)
    integration = ci.load(directory / 'integration.json')
    validate_policy(integration, integration['tests'])
    # Inventory authority is actual independently reviewed test source. No report
    # can omit cases merely by advertising a smaller self-consistent identity set.
    inventory = policy_inventory()
    require(policy_report['tests'] == inventory['policy'] and integration['tests'] == inventory['integration'],
            'policy execution omitted discovered identities')
    # Run executable qualification serially; source generation never overlaps
    # these candidate consumers or the historical shard tree.
    commands = (["make", "test-native"], ["make", "test-ada"], ["make", "check"],
                ["make", "check-ada-format"], ["alr", "-n", "-C", "ada", "build"],
                ["alr", "-n", "-C", "ada/tests", "run"],
                ["make", "test-python"], ["make", "test-rust"],
                ["sh", "docs/tools/qualify_examples.sh"])
    for i, command in enumerate(commands):
        execute(command, ROOT, logs / ('language-' + str(i) + '.log'), env)
    if (ROOT / 'docs/tools/test_ir_counter_source_policy.py').exists():
        execute(['python3', 'docs/tools/test_ir_counter_source_policy.py'], ROOT, logs / 'ir-counter-policy.log', env)
    if (ROOT / 'docs/tests/test_bootstrap_ci_policy.py').exists():
        execute([py, '-m', 'unittest', 'discover', '-s', 'docs/tests', '-p', 'test_bootstrap_ci_policy.py', '-v'],
                ROOT, logs / 'inherited-ci002.log', env)
    # Candidates cannot alter their source while executing language/test code.
    require(context() == outer and classify(ROOT, outer['target_sha'], outer['source_sha']) == value,
            'candidate qualification changed source or epoch')
    language_evidence = {str(i): {'command': command, 'exit': 0, 'log_sha256': ci.file_sha(logs / ('language-' + str(i) + '.log'))}
                         for i, command in enumerate(commands)}
    candidate = epoch.qualify_evidence(ROOT, value, directory / 'candidate', prefix)
    # verify_regenerated normally reclassifies feature-only. Policy proposals
    # also regenerate through the already verified policy-only classification.
    with tempfile.TemporaryDirectory(prefix='pol002-candidate-verify-') as temp:
        again = epoch.qualify_evidence(ROOT, value, Path(temp) / 'candidate', prefix)
        require(again == candidate, 'independent candidate epoch mismatch')
    require(context() == outer, 'source changed during candidate generation')
    historical = {'commit': HISTORY, 'tree': HISTORY_TREE,
                  'seal': seal, 'pristine': file_map(history / '_site'),
                  'published': file_map(history / '_site-published'),
                  'context': ci.load(history / 'qualification/manifest.json')['identity'],
                  'inventory': ci.load(history / 'qualification/inventory.json')}
    record = {'schema': 2, 'identity': outer, 'classification': value,
              'historical': historical, 'candidate': candidate,
              'policy_tests': policy_report['tests'], 'policy_sha256': ci.file_sha(directory / 'policy.json'),
              'integration_tests': integration['tests'], 'integration_sha256': ci.file_sha(directory / 'integration.json'),
              'languages': language_evidence}
    write(directory / 'epochs.json', record)
    # Exclude clone internals and mutable build outputs from transfer; inner seal
    # already contains the fully qualified historical payload and script result.
    payload = {'historical.tar': ci.file_sha(directory / 'historical.tar'),
               'epochs.json': ci.file_sha(directory / 'epochs.json'),
               'policy.json': ci.file_sha(directory / 'policy.json'),
               'integration.json': ci.file_sha(directory / 'integration.json')}
    for name in ('pristine', 'published'):
        payload.update({'candidate/' + name + '/' + p: h for p, h in file_map(directory / 'candidate' / name).items()})
    payload['candidate/epoch.json'] = ci.file_sha(directory / 'candidate/epoch.json')
    for name in logs.iterdir():
        if name.is_file():
            payload['logs/' + name.name] = ci.file_sha(name)
    manifest = {'schema': 2, 'identity': outer, 'epochs_sha256': digest(record), 'payload': payload}
    write(directory / 'manifest.json', manifest)
    with tarfile.open(directory / 'inputs.tar', 'w') as archive:
        for name in sorted(payload):
            archive.add(directory / name, arcname=name, recursive=False)
        archive.add(directory / 'manifest.json', arcname='manifest.json', recursive=False)
    outputs = {'bundle_sha': ci.file_sha(directory / 'inputs.tar'), 'manifest_sha': digest(manifest)}
    write(directory / 'sealed.json', outputs)
    if os.environ.get('GITHUB_OUTPUT'):
        with open(os.environ['GITHUB_OUTPUT'], 'a') as stream:
            for k, v in outputs.items():
                stream.write(k + '=' + v + '\n')
    return record


def policy_inventory():
    import importlib.util
    import unittest
    result = {}
    for key, name in (('policy', 'pol002_source_tests'), ('integration', 'pol002_epoch_tests')):
        spec = importlib.util.spec_from_file_location(name, ROOT / ('docs/tests/' + name + '.py'))
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        suite = unittest.defaultTestLoader.loadTestsFromModule(module)
        # Direct script execution uses __main__; normalize only that module name.
        result[key] = sorted('__main__.' + t.id().split('.', 1)[1] for t in ci.flatten(suite))
    return result


def verify(directory, manifest_sha):
    directory = Path(directory)
    manifest = ci.load(directory / 'manifest.json')
    require(manifest['schema'] == 2 and digest(manifest) == manifest_sha, 'outer manifest digest/schema mismatch')
    require(manifest['identity'] == context(), 'outer source/run/attempt mismatch')
    for name, expected in manifest['payload'].items():
        path = directory / name
        require(not Path(name).is_absolute() and '..' not in Path(name).parts, 'unsafe payload path')
        require(not any(p.is_symlink() for p in (path, *path.parents)) and path.is_file(), 'nonregular payload')
        require(ci.file_sha(path) == expected, 'sealed payload changed: ' + name)
    record = ci.load(directory / 'epochs.json')
    require(digest(record) == manifest['epochs_sha256'] and record['identity'] == manifest['identity'], 'mixed epoch record')
    require(record['classification'] == classify(ROOT, record['identity']['target_sha'], record['identity']['source_sha']),
            'source classification changed')
    authority(ROOT, record['classification'])
    require(record['historical']['commit'] == HISTORY and record['historical']['tree'] == HISTORY_TREE, 'wrong historical epoch')
    value = record['candidate']
    require(value['candidate_commit'] == record['identity']['source_sha'] and
            value['candidate_tree'] == record['identity']['source_tree'], 'wrong candidate epoch')
    require(ci.load(directory / 'candidate/epoch.json') == value, 'substituted candidate manifest')
    require(all(value.get(k) == v for k, v in record['classification'].items()), 'candidate source delta mismatch')
    require(file_map(directory / 'candidate/pristine') == value['pristine'] and
            file_map(directory / 'candidate/published') == value['publication']['files'], 'substituted candidate artifact')
    require(value['status'] == 'mechanically-qualified-pending-independent-review' and
            value['independent_approval'] is False and value['publication_epoch_approved'] is False,
            'candidate manifest cannot grant independent approval')
    require(ci.file_sha(directory / 'policy.json') == record['policy_sha256'], 'policy report changed')
    validate_policy(ci.load(directory / 'policy.json'), record['policy_tests'])
    require(ci.file_sha(directory / 'integration.json') == record['integration_sha256'], 'integration report changed')
    validate_policy(ci.load(directory / 'integration.json'), record['integration_tests'])
    inventory = policy_inventory()
    require(record['policy_tests'] == inventory['policy'] and record['integration_tests'] == inventory['integration'],
            'sealed policy inventory differs from actual source')
    require(set(record['languages']) == {str(i) for i in range(9)} and
            all(v['exit'] == 0 and ci.file_sha(directory / ('logs/language-' + k + '.log')) == v['log_sha256']
                for k, v in record['languages'].items()), 'missing/failed language qualification')
    verify_historical_bundle(directory, record)
    return record


def verify_historical_bundle(directory, record):
    """Recover inner authority from its authenticated seal, not outer claims."""
    with tarfile.open(Path(directory) / 'historical.tar') as archive:
        names = [m.name for m in archive.getmembers()]
        require(len(names) == len(set(names)), 'duplicate historical archive member')
        manifest = json.loads(archive.extractfile('qualification/manifest.json').read())
        inv = json.loads(archive.extractfile('qualification/inventory.json').read())
        scripts = json.loads(archive.extractfile('qualification/scripts.json').read())
        require(ci.digest(manifest) == record['historical']['seal']['manifest_sha256'], 'historical manifest substitution')
        require(ci.file_sha(Path(directory) / 'historical.tar') == record['historical']['seal']['bundle_sha256'],
                'historical bundle substitution')
        require(manifest['identity'] == record['historical']['context'] and inv == record['historical']['inventory'],
                'historical source/inventory substitution')
        require(manifest['inventory_sha256'] == ci.digest(inv) and manifest['scripts_sha256'] == ci.digest(scripts),
                'historical execution evidence mismatch')
        ci.validate_report(scripts, 'scripts', inv['groups']['scripts'], manifest['identity'], ci.digest(inv))
        require(manifest['identity']['source_sha'] == HISTORY and manifest['identity']['source_tree'] == HISTORY_TREE,
                'historical epoch substituted')
        for prefix, field in (('_site/', 'pristine'), ('_site-published/', 'published')):
            require({n[len(prefix):]: h for n, h in manifest['payload'].items() if n.startswith(prefix)}
                    == record['historical'][field], 'historical publication assignment mismatch')
    import build_enhanced_site as p
    require(p.fingerprint(record['historical']['pristine']) == p.PRISTINE_SHA256 and
            p.fingerprint(record['historical']['published']) == p.PUBLICATION_SHA256,
            'historical fixed publication identity mismatch')


def unpack(args):
    require(ci.file_sha(args.bundle) == args.bundle_sha, 'outer bundle digest mismatch')
    directory = Path(args.directory)
    require(not directory.exists(), 'unpack destination must be fresh')
    with tarfile.open(args.bundle) as archive:
        members = archive.getmembers()
        names = [m.name for m in members]
        require(len(names) == len(set(names)), 'duplicate outer archive member')
        manifest = json.loads(archive.extractfile('manifest.json').read())
        require(digest(manifest) == args.manifest_sha, 'outer archive manifest mismatch')
        require(set(names) == set(manifest['payload']) | {'manifest.json'}, 'outer archive membership mismatch')
        for member in members:
            path = Path(member.name)
            require(member.isfile() and not path.is_absolute() and '..' not in path.parts, 'unsafe outer archive')
        directory.mkdir(parents=True)
        for member in members:
            target = directory / member.name
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(archive.extractfile(member).read())
    return verify(directory, args.manifest_sha)


def run_shard(args):
    record = verify(args.directory, args.manifest_sha)
    require(args.artifact_id.isdigit() and re.fullmatch(r'[0-9a-f]{64}', args.bundle_sha),
            'invalid input artifact identity')
    directory = Path(args.directory).absolute()
    history = directory / ('shard-' + args.owner)
    historical_checkout(history)
    inner = record['historical']['seal']
    env = historical_environment(record['identity'])
    execute([sys.executable, 'docs/tools/documentation_ci.py', 'unpack', '--bundle', str(directory / 'historical.tar'),
             '--bundle-sha', inner['bundle_sha256'], '--manifest-sha', inner['manifest_sha256']],
            history, directory / (args.owner + '-unpack.log'), env)
    result = directory / (args.owner + '-execution.json')
    execute([sys.executable, 'docs/tools/documentation_ci.py', 'run', '--owner', args.owner,
             '--output', str(result), '--artifact-id', args.artifact_id,
             '--bundle-sha', inner['bundle_sha256'], '--manifest-sha', inner['manifest_sha256']],
            history, directory / (args.owner + '.log'), env)
    report = {'schema': 2, 'owner': args.owner, 'identity': record['identity'],
              'epochs_sha256': digest(record), 'artifact_id': args.artifact_id,
              'bundle_sha256': args.bundle_sha, 'manifest_sha256': args.manifest_sha,
              'execution': ci.load(result)}
    # Every runner independently reconstructs candidate source/native profiles,
    # all type links/catalog entries and manifest, not just output hash lists.
    verify_candidate(directory, record)
    verify(args.directory, args.manifest_sha)
    write(args.output, report)


def reconcile(record, reports, qualified, needs):
    require(set(needs) == {'prepare-documentation', 'enhanced-tests'} and
            all(v['result'] == 'success' for v in needs.values()), 'incomplete prerequisite jobs')
    require(set(reports) == set(ci.SHARDS), 'missing/unknown historical shard')
    require(qualified['artifact_id'].isdigit() and
            all(re.fullmatch(r'[0-9a-f]{64}', qualified[k]) for k in ('manifest_sha256', 'bundle_sha256')),
            'invalid aggregate artifact identity')
    outer = record['identity']
    historical_context = record['historical']['context']
    require(all(historical_context[k] == v for k, v in {
        'source_sha': HISTORY, 'source_tree': HISTORY_TREE, 'run_id': outer['run_id'],
        'run_attempt': outer['run_attempt'], 'repository': outer['repository']}.items()), 'wrong historical execution epoch')
    inner = record['historical']['seal']
    historical_qualified = {'artifact_id': qualified['artifact_id'],
                            'manifest_sha256': inner['manifest_sha256'],
                            'artifact_name': ci.artifact_name('inputs', historical_context),
                            'bundle_sha256': inner['bundle_sha256']}
    inv = record['historical']['inventory']
    require(len(inv['groups']['scripts']) == 142 and len(inv['groups']['enhanced']) == 437, 'historical inventory mismatch')
    for owner, report in reports.items():
        require(report['schema'] == 2 and report['owner'] == owner and report['identity'] == outer,
                'wrong outer result identity')
        require(report['epochs_sha256'] == digest(record), 'wrong report epoch')
        require(all(report[k] == qualified[k] for k in ('artifact_id', 'bundle_sha256', 'manifest_sha256')),
                'mixed artifact/run identity')
        ci.validate_report(report['execution'], owner, inv['shards'][owner], historical_context,
                           ci.digest(inv), historical_qualified)
    approval = publication_decision(record)
    return {'schema': 2, 'identity': outer, 'epochs_sha256': digest(record), 'qualified': qualified,
            'report_sha256': {k: digest(v) for k, v in reports.items()}, 'historical_scripts': 142,
            'historical_enhanced': 437, 'mechanically_qualified': True,
            'independent_approval': False, 'publication_authority': approval,
            'deployable': approval is not None and outer['event'] == 'push' and
            outer['qualification_mode'] == historical_context['qualification_mode'] == 'hosted'}


def publication_decision(record):
    """No output field can authorize publication; recompute from trusted Git."""
    identity = record['identity']
    if identity['event'] != 'push' or identity['qualification_mode'] != 'hosted':
        return None
    try:
        return epoch.publication_authority(
            ROOT, identity['source_sha'], target=identity['target_sha'],
            pristine=record['candidate']['pristine'], published=record['candidate']['publication']['files'])
    except ValueError:
        return None  # Mechanical gate still succeeds, but cannot deploy.


def verify_candidate(directory, record):
    import build_enhanced_site as p
    from check_doc_coverage import measure, ratchet
    from ada_examples import References
    directory = Path(directory)
    with tempfile.TemporaryDirectory(prefix='pol002-shard-candidate-') as temp:
        root = Path(temp) / 'source'
        epoch.export(ROOT, record['identity']['source_sha'], root)
        pristine = directory / 'candidate/pristine'
        report = measure(pristine, root)
        ratchet(report, (root / 'docs/gnatdoc-coverage-baseline.txt').read_text())
        require(report['native'] == record['candidate']['native'], 'candidate ledger not derived from source/native')
        references = References(root, pristine, ledger=report['native'])
        regenerated = Path(temp) / 'published'
        publication = epoch.enhance(pristine, root, report, references, regenerated)
        require(publication == record['candidate']['publication'] and
                p.files(regenerated) == p.files(directory / 'candidate/published'),
                'candidate publication differs from independent source reconstruction')


def aggregate(args):
    record = verify(args.directory, args.manifest_sha)
    reports = {}
    for path in Path(args.reports).rglob('*.json'):
        value = ci.load(path)
        require(value['owner'] not in reports, 'duplicate shard result')
        reports[value['owner']] = value
    qualified = {'artifact_id': args.artifact_id, 'bundle_sha256': args.bundle_sha, 'manifest_sha256': args.manifest_sha}
    gate = reconcile(record, reports, qualified, json.loads(os.environ['DOC_NEEDS']))
    write(args.output, gate)
    if os.environ.get('GITHUB_OUTPUT'):
        with open(os.environ['GITHUB_OUTPUT'], 'a') as stream:
            stream.write('gate_sha=' + ci.file_sha(args.output) + '\n')


def deployment(args):
    record = verify(args.directory, args.manifest_sha)
    require(os.environ['GITHUB_EVENT_NAME'] == 'push' and os.environ['GITHUB_REF'] == 'refs/heads/main',
            'only merged main may deploy')
    require(record['identity']['qualification_mode'] == 'hosted', 'local evidence cannot authorize deployment')
    require(ci.file_sha(args.gate) == args.gate_sha, 'deployment gate digest mismatch')
    gate = ci.load(args.gate)
    require(gate['mechanically_qualified'] is True and gate['deployable'] is True and
            gate['identity'] == context() and gate['epochs_sha256'] == digest(record), 'unqualified deployment epoch')
    require(gate['qualified'] == {'artifact_id': args.artifact_id, 'bundle_sha256': args.bundle_sha,
                                 'manifest_sha256': args.manifest_sha}, 'deployment artifact mismatch')
    require(set(gate['report_sha256']) == set(ci.SHARDS), 'incomplete deployment shard gate')
    approved = publication_decision(record)
    require(approved is not None and gate['publication_authority'] == approved,
            'no independently reviewed publication authority for merged source/tree')
    verify_candidate(Path(args.directory), record)


def live(args):
    """Read-only HTTP equality against the already sealed, gate-checked epoch."""
    record = verify(args.directory, args.manifest_sha)
    require(record['identity']['event'] == 'push' and args.expected_commit == record['identity']['source_sha'],
            'live source identity mismatch')
    require(publication_decision(record) is not None, 'live source epoch is not approved')
    from check_live_docs_site import base_url, get_entity, graph
    import build_enhanced_site as p
    url = base_url(args.base_url)
    require(args.attempts > 0 and args.interval >= 0, 'invalid live verification bounds')
    files = p.files(Path(args.directory) / 'candidate/published')
    graph(files)
    attempts = []
    for _ in range(args.attempts):
        results = [get_entity(url + name, url, data)[0] for name, data in files.items()]
        attempts.append(results)
        if all(r['comparison'] for r in results):
            write(args.evidence, {'identity': record['identity'], 'attempts': attempts, 'successful': True})
            return
        __import__('time').sleep(args.interval)
    write(args.evidence, {'identity': record['identity'], 'attempts': attempts, 'successful': False})
    raise ValueError('live epoch byte comparison failed')


def select_trusted_runner():
    """Export target policy for feature runs, retaining a separate policy case.

    Candidate code is read-only PR code, never a privileged target workflow.
    This selection makes the executing policy's Git identity explicit in logs.
    Independent review of policy/workflow proposals remains mandatory.
    """
    outer = context()
    value = classify(ROOT, outer['target_sha'], outer['source_sha'])
    if value['case'] == 'policy-only':
        runner = Path(__file__).absolute()
    else:
        target = ROOT / '.doc-tools/trusted-target'
        require(not target.exists(), 'trusted policy export destination must be fresh')
        epoch.export(ROOT, outer['target_sha'], target)
        runner = target / 'docs/tools/epoch_ci.py'
        require(runner.is_file(), 'target main does not contain reviewed POL-002 policy')
    if os.environ.get('GITHUB_OUTPUT'):
        with open(os.environ['GITHUB_OUTPUT'], 'a') as stream:
            stream.write('runner=' + str(runner) + '\n')
    print(json.dumps({'policy_case': value['case'], 'target_sha': outer['target_sha'],
                      'candidate_sha': outer['source_sha'], 'executing_policy': str(runner)}))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('command', choices=('prepare', 'unpack', 'run', 'aggregate', 'deployment', 'live', 'select'))
    parser.add_argument('--directory', default='.doc-tools/epochs')
    parser.add_argument('--gnatdoc-prefix', type=Path)
    parser.add_argument('--bundle')
    parser.add_argument('--bundle-sha')
    parser.add_argument('--manifest-sha')
    parser.add_argument('--artifact-id')
    parser.add_argument('--owner', choices=tuple(ci.SHARDS))
    parser.add_argument('--reports')
    parser.add_argument('--output')
    parser.add_argument('--gate')
    parser.add_argument('--gate-sha')
    parser.add_argument('--root', type=Path, default=ROOT)
    parser.add_argument('--base-url')
    parser.add_argument('--expected-commit')
    parser.add_argument('--attempts', type=int, default=10)
    parser.add_argument('--interval', type=int, default=30)
    parser.add_argument('--evidence')
    args = parser.parse_args()
    global_root(args.root)
    if args.command == 'select':
        select_trusted_runner()
    elif args.command == 'prepare':
        prepare(args.directory, args.gnatdoc_prefix)
    else:
        {'unpack': unpack, 'run': run_shard, 'aggregate': aggregate, 'deployment': deployment, 'live': live}[args.command](args)


def global_root(root):
    global ROOT
    ROOT = Path(root).absolute()


if __name__ == '__main__':
    main()
