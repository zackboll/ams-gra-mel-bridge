"""Exact DOC-CI-004 integration proposal; mechanical acceptance is not approval.

The executing, independently reviewable policy supplies these identities. No
candidate manifest, directory allowlist or publication registry is authority.
Historical action and three-method transitions remain separate mechanisms.
"""

import hashlib
from pathlib import Path

MAIN = '219b00c75210f5985d9889e8b541da2e7b658f04'
PREDECESSOR = 'e7eaba2cdf61a51f51f5a12d27e246c1041a1ce4'
PREDECESSOR_TREE = 'd90c9b33ed40ada5e363276f01ba09fd044e61d2'
MAIN_TREE = '4d5c6bb29cfbe6ac39041808e36af34cb84a8c0e'
R2 = 'aea6e3dd6a38f44243a2c7458923cdac54443f4e'
R2_TREE = 'fc02a1800500284e862f903cf9d633471792a68d'
POLICY = 'docs/tools/doc_ci004_source.py'

# Complete successor files, all nonexecutable regular files. Filled only from
# independently inspected final bytes; no hash is obtained from the candidate.
SUCCESSOR = {'.github/workflows/documentation.yml': '1b4be1e4cb526d58021a8aac25154faf8f8ee3e2172a98931f359d720983c711',
 'docs/ci-001r3-pol002-workflow-reconciliation.md': 'bde548cda19eb07d140f858293e454ecdb708682dd16da8204673cb93087a890',
 'docs/coverage.md': '6ae27db40a073eac85f64de64811ba13b5578c3aceab0da4debcf8404d37c977',
 'docs/doc-ci-004-integration.md': 'e72b565a7fe132ecc1564456f717155a4cc5ef2762ecc59f51c5842a08d0c9a7',
 'docs/doc-ci-004-live-shard-failure.md': '9e345649f5085c776f9e9ea6957cd52b6f92f71a3571edace00bc0ef9d305d94',
 'docs/tests/pol002_epoch_tests.py': 'b47deb49df8a05b44e0379beb8c75003d7c45462b42dcd185644e25fc0319ff0',
 'docs/tests/test_documentation_ci.py': 'b4758385bb330fc17b925aae73592d6128e263d32db50026517dc6502732b236',
 'docs/tests/test_live_docs_site.py': '76937ae947ecb2d3b7b873423abe4cceab93551b05dae281080af45b2f67d076',
 'docs/tools/ci001r3_source.py': 'c6ea1aaa385a39e01028be8187abe4cd59b45653b828213a923ce02b5d28cfa1',
 'docs/tools/ci001r3_workflow.py': 'db88551ac6e9ed522e67ddee3075a3e305025df65a877b0fca9abe643d27cee9',
 'docs/tools/documentation_ci.py': 'b97d8ef68c6ed6788179615439cc91afd3e606200e036b775942f5e740934f0f',
 'docs/tools/epoch_ci.py': '5abce6af01161a8d7f196bc53cd1f687345262a18c2436d7084aac1c4df078b5',
 'docs/tools/test_actions_dependencies.py': '208caa1df12fbd197424145a87b2612e69c57e04338518ae43d9cf187fcee012',
 'scripts/check_doc_source_integrity.py': '3e8d60e64765321153b2e1d7683d0ee0e2c8cdbe3e9860835778884a7922f602'}


def diagnostic_transition(root, target, candidate, old, new, changed, source):
    def require(condition, message):
        source.require(condition, 'DOC-CI-004 integration: ' + message)

    def sha(data):
        return hashlib.sha256(data).hexdigest()

    if POLICY not in new:
        return set()
    # A feature successor carrying this policy unchanged still uses the trusted
    # ordinary feature route. Never reapply the old action migration to main.
    if not any(old.get(name) != new.get(name) for name in set(SUCCESSOR) | {POLICY}):
        return set()
    # R2 is read-only provenance, not a required unpublished object on hosted
    # runners. Recovery verified it independently; main/#137 are published Git
    # predecessor authority. Recovered bytes are pinned below in SUCCESSOR.
    for ref, tree in ((MAIN, MAIN_TREE), (PREDECESSOR, PREDECESSOR_TREE)):
        require(source.git(root, 'rev-parse', ref + '^{tree}').decode().strip() == tree,
                'immutable predecessor tree differs: ' + ref)
    # Comparing the complete tree binds all predecessor source identities and
    # modes, including every protected file. A fixture must commit that tree.
    require(old in (source.tree(root, MAIN), source.tree(root, PREDECESSOR)),
            'not a complete verified predecessor checkout')
    require(set(changed) <= set(SUCCESSOR) | {POLICY}, 'unrelated source or policy delta')
    require(new.get(POLICY, ('',))[0] == '100644' and
            source.blob(root, candidate, POLICY) == Path(__file__).read_bytes(),
            'substituted executing transition policy')
    require(SUCCESSOR, 'successor identities not frozen')
    for name, expected in SUCCESSOR.items():
        require(new.get(name, ('',))[0] == '100644' and
                sha(source.blob(root, candidate, name)) == expected,
                'incomplete/unreviewed successor bytes or mode: ' + name)
    for name in ('docs/tests/test_live_docs_site.py', 'docs/tests/test_documentation_ci.py',
                 'docs/tools/documentation_ci.py'):
        require(new.get(name) == source.tree(root, PREDECESSOR).get(name),
                'protected three-method correction changed: ' + name)
    from ci001r3_workflow import live_contract, preparation_contract, deploy_contract
    for contract in (live_contract, preparation_contract, deploy_contract):
        contract(root)
    return set(SUCCESSOR) | {POLICY}
