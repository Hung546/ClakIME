# RUN "just -l" TO VIEW ALL COMMANDS

default:
    @just --list

# build clak fcitx5 addon
build:
    cmake --build build

# build in release mode
build-release:
    cmake -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build

# install addon for current user
install:
    cmake --build build --target install-user

# restart fcitx5 daemon
restart:
    fcitx5 -r -d

# build, install and restart fcitx5
dev: build install restart

# run all cargo tests
test-unit:
    cargo test --manifest-path engine/Cargo.toml

# test typing speed and accuracy
test-speed delay="20":
    bash scripts/tests/test_speed.sh {{delay}}

# test chromium address bar typing and backspacing
test-chromium delay="15":
    bash scripts/tests/test_chromium.sh {{delay}}

# test autocomplete selection in address bar
test-autocomplete:
    bash scripts/tests/test_autocomplete_dd.sh

# test user typing scenario
test-scenario:
    bash scripts/tests/test_user_scenario.sh

# run all automated tests
test: test-unit test-scenario

# tail debug log
log:
    tail -f /tmp/clak.log

# clear debug log
clean-log:
    rm -f /tmp/clak.log

# clean build artifacts
clean:
    rm -rf build engine/target
