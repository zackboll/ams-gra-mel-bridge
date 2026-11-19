"""Domain imports preserve the existing Python API and provider boundary."""

import importlib
import itertools
import os
from pathlib import Path
import pickle
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

import ams_mel
from ams_mel import common, ir, rf


class PublicModuleTests(unittest.TestCase):
    def test_domains_partition_the_flat_api_with_identical_objects(self):
        classified = set()
        for module in (common, ir, rf):
            self.assertIs(getattr(ams_mel, module.__name__.rsplit('.', 1)[1]), module)
            self.assertIs(importlib.import_module(module.__name__), module)
            self.assertEqual(len(module.__all__), len(set(module.__all__)))
            self.assertFalse(classified.intersection(module.__all__))
            classified.update(module.__all__)
            for name in module.__all__:
                with self.subTest(module=module.__name__, export=name):
                    value = getattr(module, name)
                    self.assertIs(value, getattr(ams_mel, name))
                    self.assertEqual(value.__module__, 'ams_mel')
        self.assertEqual(classified, set(ams_mel.__all__))
        self.assertTrue({'common', 'ir', 'rf'}.isdisjoint(ams_mel.__all__))

    def test_shared_and_domain_specific_contracts_have_unambiguous_homes(self):
        self.assertEqual(set(common.__all__), {
            'AbiVersion', 'ProviderVersion', 'UciId', 'ComponentLocation',
            'ErrorKind', 'MelError', 'MFAState', 'abi_version',
        })
        self.assertIn('RFAdmin', rf.__all__)
        self.assertNotIn('Session', rf.__all__)
        for name in ('Session', 'ChannelView', 'CommandReturn', 'MelErrorCode',
                     'CalibrationControlInterface', 'SubsystemCalibration', 'CalibrationConfiguration',
                     'CalibrationConfigurationOpenError', 'CalibrationConfigurationStream',
                     'ImageStream', 'HealthStatusChannel', 'ControlChannel',
                     'CameraProtectStatus', 'CameraProtectResponse',
                     'CameraProtectCommand', 'CameraProtectCompleted',
                     'CameraProtectRejected', 'CameraProtectRequest',
                     'CameraProtectResponseOpenError', 'CameraProtectResponseStream',
                     'Quaternion', 'LineOfSightQuaternion',
                     'LineOfSightQuaternionOpenError', 'LineOfSightQuaternionStream'):
            self.assertIn(name, ir.__all__)
        # These are the same shared value objects used by implementation code,
        # including code that requires exact types instead of subclass matches.
        config = ir.StackedConfig(
            common.UciId(bytes(16), 'channel'),
            common.UciId(bytes(16), 'platform'),
            common.ComponentLocation(0.0, 0.0, 0.0, 'key', 'system'),
        )
        self.assertIs(type(config.channel_id), ams_mel.UciId)
        self.assertIs(type(config.sensor_location), ams_mel.ComponentLocation)

    def test_domain_values_keep_legacy_pickle_paths(self):
        values = (
            ir.CalibrationConfiguration((ir.SubsystemCalibration(
                common.UciId(bytes(range(16)), 'calibration µ'),
                ir.CalibrationControlInterface.CALIBRATION_STATE_COMMAND,
                ('', '校正'), (), (common.UciId(bytes(16), ''),)),)),
            common.AbiVersion(0, 1), common.UciId(bytes(16), 'label µ'),
            common.MFAState.STANDBY, common.ErrorKind.TIMEOUT,
            ir.CommsCompleted(ir.CommsTestReport(1, 2)),
            ir.SensorType(0xFFFFFFFF),
            ir.CameraProtectResponse(0xFFFFFFFF, -(1 << 63), ir.CameraProtectStatus.UNAVAILABLE),
            ir.CameraProtectCommand(0xFFFFFFFF, -(1 << 63), True),
            ir.CameraProtectCompleted(ir.CameraProtectResponse(0, -1, ir.CameraProtectStatus.UNSUPPORTED)),
            ir.CameraProtectRejected(ir.MelErrorCode(99), 'complete UTF-8 €'),
            ir.Quaternion(1.0, -0.0, 2.0, -3.0),
            ir.LineOfSightQuaternion(
                -(1 << 63), ir.Quaternion(1.0, -0.0, 2.0, -3.0),
                ir.ForwardRightDown(4.0, 5.0, 6.0)),
        )
        for value in values:
            with self.subTest(value=value):
                restored = pickle.loads(pickle.dumps(value))
                self.assertIs(type(restored), type(value))
                self.assertEqual(restored, value)
                self.assertEqual(type(restored).__module__, 'ams_mel')
        for symbol in (ir.Session, ir.ChannelView, rf.RFAdmin, common.abi_version):
            self.assertIs(pickle.loads(pickle.dumps(symbol)), symbol)

    def test_fresh_import_orders_expose_the_same_facade(self):
        for order in itertools.permutations(('common', 'ir', 'rf')):
            script = '\n'.join(f'import ams_mel.{name}' for name in order) + '''
import ams_mel
from ams_mel import common, ir, rf
assert ams_mel.Session is ir.Session
assert ams_mel.RFAdmin is rf.RFAdmin
assert ams_mel.MFAState is common.MFAState
assert ams_mel.abi_version is common.abi_version
assert ams_mel.Quaternion is ir.Quaternion
assert ams_mel.CameraProtectResponse is ir.CameraProtectResponse
assert ams_mel.CameraProtectStatus is ir.CameraProtectStatus
assert ams_mel.CameraProtectResponseStream is ir.CameraProtectResponseStream
assert ams_mel.CameraProtectResponseOpenError is ir.CameraProtectResponseOpenError
assert ams_mel.CalibrationConfiguration is ir.CalibrationConfiguration
assert ams_mel.SubsystemCalibration is ir.SubsystemCalibration
assert ams_mel.CalibrationControlInterface is ir.CalibrationControlInterface
assert ams_mel.CalibrationConfigurationStream is ir.CalibrationConfigurationStream
assert ams_mel.CalibrationConfigurationOpenError is ir.CalibrationConfigurationOpenError
assert ams_mel.LineOfSightQuaternion is ir.LineOfSightQuaternion
assert ir.ForwardRightDown is ams_mel.ForwardRightDown
assert all(getattr(ams_mel, name).__module__ == 'ams_mel'
           for name in ams_mel.__all__)
'''
            with self.subTest(order=order):
                result = subprocess.run(
                    [sys.executable, '-W', 'error', '-c', script],
                    capture_output=True, text=True, timeout=30,
                )
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_each_namespace_retains_the_explicit_native_library_requirement(self):
        environment = dict(os.environ)
        environment.pop('AMS_MEL_NATIVE_LIB', None)
        for name in ('common', 'ir', 'rf'):
            with self.subTest(module=name):
                result = subprocess.run(
                    [sys.executable, '-W', 'error', '-c', f'import ams_mel.{name}'],
                    env=environment, capture_output=True, text=True, timeout=30,
                )
                self.assertNotEqual(result.returncode, 0)
                self.assertIn('ImportError', result.stderr)
                self.assertIn('AMS_MEL_NATIVE_LIB', result.stderr)

    def test_domain_consumers_share_values_and_errors_with_native_owners(self):
        providers = Path(os.environ['AMS_MEL_TEST_PROVIDER_DIR'])
        with tempfile.TemporaryDirectory(prefix='ams-mel-python-modules-') as folder:
            log = Path(folder) / 'lifetime.log'
            log.touch()
            with mock.patch.dict(os.environ, {'AMS_MEL_TEST_LIFETIME_LOG': str(log)}):
                self.assertIs(type(common.abi_version()), common.AbiVersion)
                with ir.Session.open(providers / 'libmock_ir_provider.so',
                                     'success', 'aperture-A') as session:
                    self.assertIs(type(session.provider_version()), common.ProviderVersion)
                with self.assertRaises(common.MelError) as caught:
                    session.provider_version()
                self.assertIs(caught.exception.kind, common.ErrorKind.INVALID_ARGUMENT)
                with rf.RFAdmin.open(providers / 'libmock_rf_provider.so',
                                     'admin:ok') as admin:
                    self.assertIs(admin.command_state(common.MFAState.STANDBY), True)
                self.assertFalse(session.is_open)
                self.assertFalse(admin.is_open)


if __name__ == '__main__':
    unittest.main()
