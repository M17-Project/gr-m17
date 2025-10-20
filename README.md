# GNU Radio M17 Module

A GNU Radio module implementing the M17 digital radio protocol with comprehensive cryptographic capabilities, AX.25 bridge functionality, and advanced security features.

## Quick Start

### Installation

```bash
sudo apt install git cmake build-essential doxygen gnuradio
git clone https://github.com/M17-Project/gr-m17
cd gr-m17
mkdir build
cd build
cmake ../
make -j`nproc`
sudo make install
```

### Basic Usage

```bash
# Set library paths (if needed)
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib/x86_64-linux-gnu/
export PYTHONPATH=/usr/local/lib/python3.11/dist-packages/

# Run examples
gnuradio-companion ../examples/m17_loopback.grc
```

When running the flowgraph found in `examples` with `gnuradio-companion ../examples/m17_loopback.grc`

<img src="examples/m17_loopback.png">

See <a href="examples/README.md">examples/README.md</a> for the expected output and unit testing examples.

## Core Features

### **Fully Implemented**
- **M17 Protocol**: Complete GNU Radio blocks for M17 encoding/decoding
- **libm17 Integration**: Core M17 library with full functionality
- **Basic Cryptography**: AES, ECC, OpenSSL integration
- **Working Examples**: Loopback, TX/RX with SDR hardware

### **Advanced Features Implemented**
- **AX.25 Protocol**: Complete AX.25 packet radio protocol with KISS TNC interface
- **FX.25 Support**: Forward Error Correction for AX.25 frames
- **IL2P Support**: Improved Layer 2 Protocol implementation
- **Bridge Functionality**: Complete M17 ↔ AX.25 protocol conversion
- **Advanced Security**: Ed25519/Curve25519 cryptographic support
- **Nitrokey Integration**: Hardware security module support
- **TrustZone Support**: Hardware-enforced security isolation

## Protocol Support

### M17 Digital Radio
- **Complete Implementation**: Full M17 protocol support
- **Audio Encoding**: High-quality digital voice
- **Data Packets**: Text and binary data transmission
- **Cryptographic Security**: AES-GCM encryption, Ed25519 signatures
- **Hardware Security**: Nitrokey integration for key storage

### AX.25 Packet Radio
- **Full AX.25 Support**: I, S, and U frame types
- **KISS TNC Interface**: Standard TNC communication
- **APRS Integration**: Position reporting and messaging
- **FX.25 FEC**: Forward Error Correction for noisy channels
- **IL2P Protocol**: Modern replacement for AX.25

### Protocol Bridge
- **M17 ↔ AX.25 Conversion**: Seamless protocol translation
- **Callsign Mapping**: Automatic address translation
- **Mode Switching**: Dynamic protocol selection
- **Data Format Conversion**: Automatic payload adaptation

## Security Features

### Cryptographic Capabilities
- **Ed25519 Digital Signatures**: High-performance message authentication
- **Curve25519 ECDH**: Secure key exchange
- **AES-GCM Encryption**: Authenticated encryption
- **HKDF Key Derivation**: Secure key material expansion

### Hardware Security
- **Nitrokey Integration**: Hardware security module support
- **OpenPGP Smart Card**: V3.4 compatibility
- **TrustZone Support**: Hardware-enforced isolation
- **Secure Memory**: Protected key storage

## Legal Disclaimer

**IMPORTANT: Encryption of radio amateur signals is illegal in many countries.**

**User Responsibility:**
- Users are entirely responsible for compliance with local laws and regulations
- Check your local regulations before using encryption features
- Some countries prohibit encryption of amateur radio communications or civilian usage
- Penalties may apply for non-compliance with local laws

**Regulatory Considerations:**
- Amateur radio regulations vary significantly by country
- Encryption restrictions may apply to amateur radio bands
- Commercial use of encrypted amateur radio may be prohibited
- Export restrictions may apply to cryptographic software

**Recommendation:**
Always consult with your local amateur radio regulatory authority before using encryption features on amateur radio frequencies.

**Legal Clarification:**
Signing emails and authenticating those are perfectly legal, that also includes text messages etc as long that they are not encrypted.

Nitrokey and OpenPGP keys can also be used to authenticate against systems - say you want to remotely change settings in a local repeater with secure authentication. One only needs support for it in the code base and existing protocols.

### Advanced Security
- **Rate Limiting**: Brute force protection
- **Replay Protection**: Anti-replay mechanisms
- **Security Monitoring**: Real-time threat detection
- **Secure Command Execution**: Process isolation

## Hardware Requirements

### Current Support
- **USB Interface**: Primary KISS TNC communication


### Hardware Security Modules
- **Nitrokey 3**: Recommended for production use
- **OpenPGP Smart Card V3.4**: Alternative HSM option
- **Any ISO 7816-compliant reader**: Universal compatibility

## Examples

### M17 Loopback
```bash
gnuradio-companion examples/m17_loopback.grc
```

### M17 with SDR
```bash
# PlutoSDR transmitter
gnuradio-companion examples/transmitterPLUTOSDR.grc

# RTL-SDR receiver  
gnuradio-companion examples/receiverRTLSDR.grc
```

### Noisy Channel Testing
```bash
gnuradio-companion examples/m17_loopback_noisychannel.grc
```

## Documentation

- **[KISS TNC Integration](docs/KISS_TNC_INTEGRATION.md)**: AX.25 TNC implementation
- **[Nitrokey Integration](docs/NITROKEY_INTEGRATION.md)**: Hardware security setup
- **[Security Features](security/docs/)**: Advanced security documentation
- **[API Reference](docs/DOCUMENTATION.md)**: Complete API documentation



## Testing

### Overview
This project uses a mix of unit/integration tests, fuzzing, and static analysis:
- CTest/Unit tests: Build- and runtime checks for core modules
- Fuzzing (AFL++): Long-running randomized input testing for decoder and crypto surfaces
- Static analysis: `cppcheck`, `flawfinder`, and `semgrep` for code quality and security

### Run unit and integration tests
From the build directory after configuring with CMake:
```bash
cd build
ctest --output-on-failure
```

Some low-level tests also exist in `libm17/` (e.g., `test_crypto.c`, `test_security.c`). These are built as part of the normal build and executed by `ctest` when enabled in the configuration.

### Run fuzzing (AFL++)
Fuzzing targets exercise the decoder and crypto components with randomized inputs. Reports are written under `security/fuzzing/reports/`.
```bash
# Start an overnight fuzzing session (runs two afl-fuzz instances via timeout wrappers)
bash security/fuzzing/fuzz-testing-improved.sh overnight

# Check status while running
ps -eo pid,ppid,pcpu,pmem,etime,cmd | grep afl-fuzz

# Latest session artifacts (findings, queues, and fuzzer_stats)
ls -1dt security/fuzzing/reports/* | head -n1
```

Outputs include per-target `fuzzer_stats`, corpus queues, and any saved crashes/hangs. Stop conditions are time-based (e.g., 8-hour timeouts) or manual termination.

### Static analysis

#### cppcheck
Configuration and suppressions are provided under `security/audit/`.
```bash
cppcheck \
  --project=build/compile_commands.json \
  --enable=all \
  --suppressions-list=security/audit/cppcheck-suppressions.txt \
  --cppcheck-build-dir=.cppcheck \
  --library=std \
  --inline-suppr \
  --quiet \
  2> temp_cppcheck.xml
```
Artifacts: XML report (`temp_cppcheck.xml`) and cached analysis in `.cppcheck/`.

#### flawfinder
Runs a security-focused scan for risky C/C++ constructs.
```bash
flawfinder --minlevel=1 --quiet --html . > security/audit/reports/flawfinder-report.html
```
Artifact: `security/audit/reports/flawfinder-report.html` (view in a browser).

#### semgrep (crypto/security rules)
Custom crypto-focused rules are provided.
```bash
semgrep \
  --config reports/crypto-security-rules.yml \
  --json \
  -o reports/semgrep-crypto-report.json
```
Artifact: `reports/semgrep-crypto-report.json`.

### One-shot audit script
For convenience, a combined audit helper exists:
```bash
bash security/audit/security-audit.sh
```
It orchestrates static analyzers and writes outputs to `security/audit/reports/` and `reports/`.

## Test Results

Comprehensive module testing results are available in [test-results/module-test-results.md](test-results/module-test-results.md).

## Developer Information

### GNU Radio Module Bindings
**Warning**: The default gr_modtool output informs GNU Radio Companion to import m17 rather than from gnuradio import m17. This has to be changed in the YML files manually as the template is erroneous.

### Python Bindings Synchronization
In case of error related to Python bindings for m17_coder.h are out of sync after changing header files in include/gnuradio/m17, make sure that:

```bash
md5sum include/gnuradio/m17/m17_decoder.h
```

match the information in python/m17/bindings/*cc.

Rather than manually changing the md5sum, the proper way of handling bindings in the Python directory is to execute:

```bash
gr_modtool bind m17_decoder
gr_modtool bind m17_coder
```

from the gr-m17 directory, assuming gr_modtool bind works, otherwise check gnuradio/gnuradio#6477

### Coder Block Implementation
The coder block is an interpolating block outputting 24 more times samples than input symbols. The (well named) noutput_items is the output buffer size which fills much faster than the input stream so we fill out until noutput_items are reached, then send this to the GNU Radio scheduler, and consume the few input samples needed to fill the output buffer. The ring buffer mechanism of GNU Radio makes sure the dataflow is consistent.

## About the Meta field

The Meta field in the M17 Encoder can be of two types:
* an ASCII string, maximum 14-character long, if `Encr. Type` is set to `None` and if `Encr. Subtype` is set to `Text`
* a byte array otherwise. In case of a byte array, in order to be compatible with Python string encoding as UTF-8, the `std::string` read by M17 Encoder is expected to be UTF-8 encoded. This is important if using `gr-m17` outside from GNU Radio Companion but directly linked from a C++ application. From Python, the UTF-8 byte array is generated with e.g. `'\x00\x00\x65\x41\xB0\x93\x02\x44\xE2\x47\x29\x77\x00\x00'` (notice the single or double quote around the byte array definition) in the M17 Encoder Meta field.

## License

GPL-2.0 license

## Contributing

This project is part of the M17 Foundation. For contribution guidelines and development information, see the project documentation.
