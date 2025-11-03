import pytest

def pytest_addoption(parser: pytest.Parser):
    parser.addoption(
        "--path-to-packer",
        action="store",
        default=None,
        help="Path to the packer binary to use for integration tests.",
    )

