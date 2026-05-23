# Birthday Calendar

> ⚠️ **Warning:** This project is vibecoded dogshit. Run at your own risk.
> Then again, stuff meant for a magic mirror on the toilet should not be mission critical, right?

Self-hosted birthday calendar app with:
- A modern Svelte frontend that shows birthdays grouped by month from a shared iCal feed.
- A C++ backend API that fetches and parses iCal data.
- A plus-button flow for submitting a missing birthday as a generated vCard.
- Pluggable notifier channels (Strategy pattern): SMTP (Gmail app password) and file-based fallback.
- Temporary vCard storage that auto-cleans when birthdays appear in the iCal feed.
- JWT-based authentication with rotating QR tokens for local access.
- Dockerized deployment and GitHub Actions image publishing.

## Architecture

- Frontend: Svelte 5 + Vite (`frontend/`)
- Backend: C++20 + `cpp-httplib` + `libcurl` + `nlohmann/json` + OpenSSL (`backend/`)
- Temporary storage: `storage/pending/` (pending vCards)
- JWT signing secret: `storage/.jwt_secret` (auto-generated on first run)

## Authentication

The app uses JWT-based authentication with a rotating QR code intended for display on a local Magic Mirror device (e.g. via [MMM-QRCode](https://github.com/uxigene/MMM-QRCode)).

**Flow:**
1. Magic Mirror polls `GET /api/auth/qr` (local network only) and displays the returned `authUrl` as a QR code.
2. User scans QR with phone. Browser opens `PUBLIC_BASE_URL/auth?token=...` (public address).
3. Token is exchanged via `POST /api/auth/exchange`, which sets an HTTP-only session cookie.
4. Subsequent requests are authenticated via the cookie.

**QR token rotation:**
- Tokens rotate on a configurable window (`JWT_TOKEN_LIFETIME_SECONDS`, default 30s).
- A grace period (`JWT_ROTATION_GRACE_SECONDS`, default 30s) allows scanning near rotation boundaries.
- Session tokens last independently (`JWT_SESSION_LIFETIME_SECONDS`, default 1h).

**`/api/auth/qr` is restricted to local network clients** via CIDR allowlist (`LOCAL_QR_ALLOWED_CIDRS`).

## Notifier Channels

SMTP notifier activates automatically if all four SMTP env vars are set (`SMTP_USERNAME`, `SMTP_PASSWORD`, `MAIL_FROM`, `MAIL_TO`). File notifier always runs as fallback and writes `.vcf` files to `storage/pending/`.

Both notifiers run on each submission. If all notifiers fail, the API returns 502.

## Environment Variables

Copy `.env.example` to `.env` and update values.

| Variable | Default | Description |
|---|---|---|
| `ICAL_URL` | *(required)* | Public/shared iCal URL |
| `SMTP_HOST` | `smtp.gmail.com` | SMTP host |
| `SMTP_PORT` | `587` | SMTP port |
| `SMTP_USERNAME` | | SMTP username (activates SMTP notifier) |
| `SMTP_PASSWORD` | | SMTP password / Gmail app password |
| `MAIL_FROM` | | From address |
| `MAIL_TO` | | Recipient address |
| `AUTH_ENABLED` | `true` | Enable JWT auth. Set to `false` for local dev only |
| `PUBLIC_BASE_URL` | *(required when auth enabled)* | Public base URL, e.g. `https://your-domain.com` |
| `JWT_SECRET` | | Static JWT signing secret. Omit to auto-generate |
| `JWT_SECRET_FILE` | `./storage/.jwt_secret` | Path to persist auto-generated secret |
| `JWT_ISSUER` | `birthday-calendar` | JWT issuer claim |
| `JWT_COOKIE_NAME` | `birthday_auth` | Session cookie name |
| `JWT_TOKEN_LIFETIME_SECONDS` | `30` | QR token rotation window |
| `JWT_ROTATION_GRACE_SECONDS` | `30` | Grace period around rotation boundary |
| `JWT_SESSION_LIFETIME_SECONDS` | `3600` | Session cookie lifetime (1h) |
| `LOCAL_QR_ALLOWED_CIDRS` | *(LAN ranges)* | Comma-separated CIDRs allowed to fetch QR tokens |
| `PORT` | `8080` | Backend listen port |
| `PUBLIC_DIR` | `../frontend/dist` | Path to built frontend assets |
| `PENDING_DIR` | `./storage/pending` | Path to pending vCard storage |

## Local Development

### Dev Container (recommended)

This repo includes a ready-to-use devcontainer with Node.js and C++ build tools.

1. Open the folder in VS Code.
2. Run **Reopen in Container**.
3. Wait for post-create setup to install frontend dependencies and configure CMake.

Inside the devcontainer:

```bash
# Build frontend
cd frontend && npm install && npm run build && cd ..

# Build and run backend (serves API + frontend)
cmake --build backend/build -j
AUTH_ENABLED=false ./backend/build/birthday_backend
```

Backend runs on `http://localhost:8080` and serves both API and built frontend.

> **Note:** Set `AUTH_ENABLED=false` for local dev unless you have a `PUBLIC_BASE_URL` accessible from your phone.

### Optional: Frontend hot-reload with Vite

Only needed when iterating on frontend changes:

```bash
cd frontend && npm run dev
```

Vite dev server proxies `/api` to the backend.

### Backend build only

```bash
cmake -S backend -B backend/build -DCMAKE_BUILD_TYPE=Release
cmake --build backend/build -j
./backend/build/birthday_backend
```

## Docker

```bash
cp .env.example .env
# Edit .env — set ICAL_URL and PUBLIC_BASE_URL at minimum
docker compose up --build
```

Container exposes port `8080`.

## API

All endpoints except `/api/health`, `/api/auth/qr`, and `/api/auth/exchange` require a valid session cookie.

| Method | Path | Auth | Description |
|---|---|---|---|
| `GET` | `/api/health` | No | Health check |
| `GET` | `/api/auth/qr` | Local only | Get rotating QR token + auth URL |
| `POST` | `/api/auth/exchange` | No | Exchange QR token for session cookie |
| `GET` | `/api/birthdays` | Yes | List birthdays (iCal + pending) |
| `POST` | `/api/vcards` | Yes | Submit a missing birthday vCard |

### POST /api/vcards

```json
{
  "firstName": "Ada",
  "lastName": "Lovelace",
  "email": "ada@example.com",
  "birthday": "1815-12-10",
  "notes": "Please add this birthday"
}
```

### GET /api/auth/qr (local network only)

```json
{
  "token": "<jwt>",
  "expiresAt": 1748000000,
  "authUrl": "https://your-domain.com/auth?token=<jwt>"
}
```

## Data Lifecycle

1. User submits a missing birthday via the + button form.
2. Backend validates and generates a vCard.
3. All configured notifiers run (SMTP sends email; File writes `.vcf` to `storage/pending/`).
4. On each `GET /api/birthdays`, pending vCards whose birthday month/day now appears in the iCal feed are automatically deleted.

## GitHub Actions

Workflow: `.github/workflows/docker-image.yml`

On push to `main`, builds and publishes image to `ghcr.io/<owner>/<repo>`. Pull requests build without publishing.
