# Birthday Calendar

Self-hosted birthday calendar app with:
- A modern Svelte frontend that shows birthdays from a shared iCal feed.
- A C++ backend API that fetches and parses iCal data.
- A plus-button flow for submitting a missing birthday as a generated vCard.
- Pluggable sender channels (Strategy pattern) with SMTP (Gmail app password) and file-based fallback.
- Temporary vCard storage that auto-cleans when birthdays show up in the iCal feed.
- Dockerized deployment and GitHub Actions image publishing.

## Architecture

- Frontend: Svelte + Vite (`frontend/`)
- Backend: C++20 + `cpp-httplib` + `libcurl` + `nlohmann/json` (`backend/`)
- Temporary storage: `storage/pending/`
- Outbox fallback: `storage/outbox/`

## Sender Channel Pattern

The backend uses a sender abstraction:
- `SenderChannel` interface
- `SmtpSenderChannel`: sends email through SMTP (Gmail supported via app password)
- `FileSenderChannel`: writes outbound email intent to a local file for testing/fallback

Choose channel via environment variable:
- `SENDER_CHANNEL=smtp` (default)
- `SENDER_CHANNEL=file`

## Environment Variables

Copy `.env.example` to `.env` and update values.

Required:
- `ICAL_URL`: public/shared iCal URL

For SMTP/Gmail:
- `SMTP_HOST` (default `smtp.gmail.com`)
- `SMTP_PORT` (default `587`)
- `SMTP_USERNAME` (your Gmail address)
- `SMTP_PASSWORD` (Gmail app password)
- `MAIL_FROM`
- `MAIL_TO`

## Local Development

### Dev Container (recommended)

This repo includes a ready-to-use devcontainer with Node.js and C++ build tools.

1. Open the folder in VS Code.
2. Run Reopen in Container.
3. Wait for post-create setup to install frontend dependencies and configure CMake.

Inside the devcontainer, run:

```bash
cmake --build backend/build -j
./backend/build/birthday_backend
```

In another terminal:

```bash
cd frontend
npm run dev
```

### 1. Frontend

```bash
cd frontend
npm install
npm run dev
```

### 2. Backend

```bash
cmake -S backend -B backend/build -DCMAKE_BUILD_TYPE=Release
cmake --build backend/build -j
./backend/build/birthday_backend
```

Backend runs on `http://localhost:8080`.
Frontend dev server runs on `http://localhost:5173` and proxies `/api` to backend.

## Docker

```bash
docker compose up --build
```

Container exposes port `8080` and serves both API and built frontend.

## Data Lifecycle

1. User submits missing birthday through the plus-button form.
2. Backend creates a vCard and stores it in `storage/pending`.
3. Backend sends the vCard via configured sender channel.
4. On birthday feed refresh (`GET /api/birthdays`), pending vCards whose birthday month/day now appears in iCal are automatically deleted.

## API

- `GET /api/health`
- `GET /api/birthdays`
- `POST /api/vcards`

Example payload for `POST /api/vcards`:

```json
{
  "firstName": "Ada",
  "lastName": "Lovelace",
  "email": "ada@example.com",
  "birthday": "1815-12-10",
  "notes": "Please add this birthday"
}
```

## GitHub Actions

Workflow: `.github/workflows/docker-image.yml`

On push to `main`, it builds and publishes image to:
- `ghcr.io/<owner>/<repo>`

For pull requests, it builds without pushing.
