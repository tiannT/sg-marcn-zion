let
  source = builtins.fetchTarball {
    url = "https://github.com/NixOS/nixpkgs/archive/c0bebd16e69e631ac6e52d6eb439daba28ac50cd.tar.gz";
    sha256 = "1fbhkqm8cnsxszw4d4g0402vwsi75yazxkpfx3rdvln4n6s68saf";
  };
  pkgs = import source { crossSystem.config = "riscv64-unknown-linux-gnu"; };
in pkgs.stdenv.mkDerivation {
  pname = "planb-tee-apps";
  version = "1.0";
  src = ../src/macrn/tools/sg2044/apps;
  buildInputs = [ pkgs.openssl ];
  dontConfigure = true;
  buildPhase = ''
    $CC -std=c11 -O2 -Wall -Wextra -Werror tee_crypto.c -lcrypto -o tee-crypto
    $CC -std=c11 -O2 -Wall -Wextra -Werror tee_vault.c -lcrypto -o tee-vault
  '';
  installPhase = ''
    mkdir -p $out/bin
    cp tee-crypto tee-vault $out/bin/
  '';
}
