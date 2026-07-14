# Git, Remotes und Versionierung

## Ein Repository pro Library

Jede Library und die Firmware besitzen ein eigenes Repository. Der Workspace-Ordner kann zusätzlich als Meta-Repository für Dokumentation, Skripte und Vorlagen versioniert werden.

Wichtig: `repositories/` bleibt im Workspace-Meta-Repository ignoriert. Dadurch gibt es keine verschachtelte Versionsverwaltung im Root-Repo und jedes Modul kann separat getaggt und veröffentlicht werden.

## Gitea/Forgejo automatisch einrichten

Für Gitea oder Forgejo ist die API-Variante am bequemsten: Das Skript legt die sieben leeren Repositories an und setzt danach in jedem lokalen Repository den Remote `origin`.

Erstelle in Gitea ein persönliches Zugriffstoken mit Schreibrecht auf Repositories. Unter zsh liest du es so ein:

```zsh
read -s "GITEA_TOKEN?Gitea-Token: "
echo
export GITEA_TOKEN
```

Danach:

```bash
./scripts/create-gitea-repositories.sh \
  'http://192.168.178.99:3000' \
  'Theo' \
  user \
  private
```

Das Skript erzeugt für alle Repositories URLs nach diesem Muster:

```text
http://192.168.178.99:3000/Theo/mea-core.git
http://192.168.178.99:3000/Theo/mea-processing.git
...
```

Prüfen und pushen:

```bash
git -C repositories/mea-core remote -v
./scripts/push-all.sh
unset GITEA_TOKEN
```

Beim `git push` über HTTP ist der Benutzername `Theo`; als Passwort verwendest du den Gitea-Token.

Wichtig: Den Token nicht direkt in die Remote-URL schreiben.

## Remotes manuell konfigurieren

```bash
./scripts/configure-remotes.sh 'git@gitserver.local:/srv/git/embedded'
./scripts/push-all.sh
```

Das Skript erzeugt URLs nach diesem Muster:

```text
git@gitserver.local:/srv/git/embedded/mea-core.git
git@gitserver.local:/srv/git/embedded/mea-processing.git
...
```

## Bare Git-Server ohne Gitea/GitLab

Auf dem Server werden bare Repositories benötigt:

```bash
sudo mkdir -p /srv/git/embedded
sudo chown -R git:git /srv/git/embedded
sudo -u git git init --bare /srv/git/embedded/mea-core.git
```

Das mitgelieferte Skript automatisiert dies für alle Repositories:

```bash
./scripts/create-bare-remotes.sh git@gitserver.local /srv/git/embedded
```

## Branches

Empfehlung für kleine Teams:

- `main`: stabiler, baubarer Stand
- `feature/<thema>`: neue Funktion
- `fix/<thema>`: Fehlerbehebung
- `release/<version>`: optional bei geplanten Releases

## Semantic Versioning

- Patch: Fehlerbehebung ohne API-Bruch, z. B. `0.1.0 -> 0.1.1`
- Minor: neue rückwärtskompatible Funktion, z. B. `0.1.1 -> 0.2.0`
- Major: inkompatible API-Änderung, z. B. `1.4.0 -> 2.0.0`

Version in `library.json` aktualisieren, committen und taggen:

```bash
git add library.json
git commit -m "chore: release 0.2.0"
git tag -a v0.2.0 -m "MEA Processing 0.2.0"
git push origin main --follow-tags
```

## Lokale Entwicklung gegenüber reproduzierbaren Releases

Während der Entwicklung verwendet die Firmware `symlink://`-Abhängigkeiten. Für Releases oder CI sollten Git-Tags oder Commit-Hashes verwendet werden:

```ini
lib_deps =
    mea-core=git+ssh://git@gitserver.local/srv/git/embedded/mea-core.git#v1.0.0
    mea-processing=git+ssh://git@gitserver.local/srv/git/embedded/mea-processing.git#v1.2.0
```

Bei sicherheitskritischen oder streng reproduzierbaren Builds sollte statt eines Branch-Namens ein Tag oder vollständiger Commit-Hash gepinnt werden.

## Commit-Konvention

Empfohlen:

```text
feat: add pressure sensor interface
fix: handle millis overflow
refactor: split sink registry
 test: add state machine recovery test
 docs: explain remote setup
chore: release 0.2.0
```
