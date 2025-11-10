# detection_droneVoici la version **markdown** de l’explication :

---

# 🧠 Pourquoi ton projet Python ne marchait pas (et comment on l’a réparé)

## 🚨 Les problèmes initiaux

### 1. Mauvais nom de package (`lib.scapy` au lieu de `scapy`)

Ton projet importait :

```python
import lib.scapy.all as scapy
```

Mais Scapy s’attend à être importé comme **`scapy`** (paquet top-level).
Du coup, ses imports internes du type :

```python
from scapy.modules.six.moves import range
```

échouaient, car Python cherchait `scapy.modules.six.moves` — un module inexistant (ton paquet s’appelait `lib.scapy`).

---

### 2. Incompatibilité du `six` embarqué avec Python 3.12

Ta copie locale de Scapy contient un fichier :

```
lib/scapy/modules/six.py
```

Ce module implémente un système d’import “magique” (`_SixMetaPathImporter`) compatible Python 2/3.
Sous Python 3.12, ce mécanisme casse : les imports `from scapy.modules.six.moves import …` ne fonctionnent plus.

---

### 3. Masquage de la bibliothèque standard

Tu avais un fichier :

```
lib/json.py
```

qui **masquait le module standard `json`** de Python.
Cela pouvait perturber d’autres imports internes (notamment ceux de Scapy).

---

## 🛠️ Les corrections effectuées

### 1. Rendre `scapy` top-level

* Ajout dans **`main.py`** (tout en haut) :

  ```python
  import os, sys
  PROJECT_ROOT = os.path.dirname(os.path.abspath(__file__))
  LIB_DIR = os.path.join(PROJECT_ROOT, "lib")
  if LIB_DIR not in sys.path:
      sys.path.insert(0, LIB_DIR)
  ```

* Modification dans **`lib/sniff.py`** :

  ```python
  import scapy.all as scapy
  ```

  (au lieu de `lib.scapy.all`)

* Restauration des **imports absolus** dans `lib/scapy/all.py` :

  ```python
  from scapy.config import *
  from scapy.base_classes import *
  ...
  ```

---

### 2. Création d’un **shim de compatibilité** `_six_compat.py`

Ajout du fichier `lib/scapy/_six_compat.py` :

```python
try:
    from scapy.modules import six as _six
except Exception:
    _six = None

if _six is None:
    try:
        import six as _six  # version standard si dispo
    except Exception:
        _six = None

if _six is None:
    # Repli minimal pour Python 3
    class _Moves:
        range = range
        zip = zip
        map = map
        class queue:
            from queue import Queue, Empty
    class _DummySix:
        moves = _Moves()
    _six = _DummySix()

moves = _six.moves
```

Puis, dans les fichiers où tu avais :

```python
from scapy.modules.six.moves import range, zip
```

on a remplacé par :

```python
from scapy._six_compat import moves
range = moves.range
zip = moves.zip
```

Et pour `scapypipes.py` :

```python
from scapy._six_compat import moves
Queue, Empty = moves.queue.Queue, moves.queue.Empty
```

👉 Ce shim garantit que `moves.range`, `moves.zip`, `moves.queue` existent même si le module `six` embarqué de Scapy échoue.

---

### 3. Éviter le masquage de la stdlib

Renommage de :

```
lib/json.py → lib/json_local.py
```

et purge des caches :

```bash
find lib -name "__pycache__" -type d -exec rm -rf {} +
```

---

## ✅ Résultat

Ton script se lance correctement :

```
Lancement du script.
Lancement du mode monitor.
Mode monitor de l'interface wlx00c0cab7db3c
impossible de passer en mode monitor, sortie du script !
```

L’erreur finale :

```
Run it as root
```

n’est **plus un bug Python** — c’est simplement `airmon-ng` qui demande des **droits administrateur** pour activer le mode “monitor”.

---

## ⚙️ Pour exécuter sans `sudo`

Tu peux accorder les droits réseau à ton interpréteur Python :

```bash
sudo setcap cap_net_raw,cap_net_admin=eip ./venv/bin/python3
```

Ensuite, tu peux lancer ton script **sans sudo** :

```bash
./venv/bin/python3 main.py
```

---

## 🧾 En résumé

| Problème                                       | Solution appliquée                                    |
| ---------------------------------------------- | ----------------------------------------------------- |
| `ModuleNotFoundError: scapy.modules.six.moves` | Ajout d’un shim `_six_compat.py`                      |
| `lib.scapy` importé au lieu de `scapy`         | Ajout de `lib` à `sys.path` et correction des imports |
| `lib/json.py` masque `json` standard           | Fichier renommé en `json_local.py`                    |
| Scapy incompatible Python 3.12                 | Patchs via `_six_compat`                              |
| Exécution sans privilèges root                 | Ajout possible des capabilities via `setcap`          |

---

🎉 **Ton Scapy local est maintenant fonctionnel sous Python 3.12**, tout en restant indépendant du paquet officiel installé via pip.
