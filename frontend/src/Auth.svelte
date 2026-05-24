<script>
    import { onMount } from "svelte";
    import QRCode from "qrcode";

    let authUrl = "";
    let token = "";
    let manual = "";
    let result = "";
    let qrDataUrl = "";
    let qrError = "";
    let loading = true;
    let expiresAt = 0;

    async function renderQr() {
        qrDataUrl = "";
        qrError = "";
        if (!authUrl) return;
        try {
            qrDataUrl = await QRCode.toDataURL(authUrl, {
                width: 300,
                margin: 1,
            });
        } catch (e) {
            qrError = "QR generation failed";
        }
    }

    onMount(async () => {
        loading = true;
        try {
            const res = await fetch("/api/auth/authUrl", {
                credentials: "include",
            });
            if (!res.ok) throw new Error("fetch failed: " + res.status);
            const data = await res.json();
            authUrl = data.authUrl;
            token = data.token;
            expiresAt = data.expiresAt || 0;
            manual = token;
            await renderQr();
        } catch (e) {
            result = "Failed to fetch authUrl: " + e.message;
        } finally {
            loading = false;
        }
    });

    function formatEpoch(epoch) {
        if (!epoch) return "";
        return new Date(epoch * 1000).toLocaleTimeString([], {
            hour: "2-digit",
            minute: "2-digit",
        });
    }

    async function exchange(t) {
        result = "";
        try {
            const r = await fetch("/api/auth/exchange", {
                method: "POST",
                headers: { "Content-Type": "application/json" },
                credentials: "include",
                body: JSON.stringify({ token: t }),
            });
            const j = await r.json();
            result = JSON.stringify({ status: r.status, body: j }, null, 2);
            if (r.ok) {
                // optionally open the app
                // window.location.href = authUrl;
            }
        } catch (err) {
            result = "Exchange failed: " + err.message;
        }
    }

    async function refreshAuth() {
        result = "";
        loading = true;
        try {
            const res = await fetch("/api/auth/authUrl", {
                credentials: "include",
            });
            if (!res.ok) throw new Error("fetch failed: " + res.status);
            const data = await res.json();
            authUrl = data.authUrl;
            token = data.token;
            expiresAt = data.expiresAt || 0;
            manual = token;
            await renderQr();
        } catch (e) {
            result = "Failed to refresh authUrl: " + e.message;
        } finally {
            loading = false;
        }
    }
</script>

<main class="shell">
    <header>
        <p class="eyebrow">Shared iCal</p>
        <h1>Authenticate</h1>
        <p class="sub">Use the local auth page to open the app securely.</p>
    </header>

    <section class="auth-panel">
        {#if loading}
            <p class="state">Loading local auth URL...</p>
        {:else}
            <div class="auth-layout">
                {#if qrDataUrl}
                    <img
                        class="qr-image"
                        alt="Auth token QR code"
                        src={qrDataUrl}
                    />
                {:else}
                    <div class="qr-placeholder">QR will appear here</div>
                {/if}

                <div>
                    <p class="state">
                        Token expires at {formatEpoch(expiresAt)}
                    </p>
                    <p class="auth-url">
                        <a href={authUrl} target="_blank" rel="noreferrer"
                            >{authUrl}</a
                        >
                    </p>

                    <textarea
                        class="token-input"
                        rows="4"
                        readonly
                        bind:value={token}
                    ></textarea>

                    <div class="auth-actions">
                        <button class="ghost" on:click={refreshAuth}
                            >Refresh QR</button
                        >
                        <a
                            class="solid link-btn"
                            href={authUrl}
                            target="_blank"
                            rel="noreferrer">Open App</a
                        >
                    </div>
                </div>
            </div>

            <hr />

            <h3>Manual token exchange</h3>
            <textarea class="token-input" rows="3" bind:value={manual}
            ></textarea>
            <div class="auth-actions">
                <button class="solid" on:click={() => exchange(manual)}>
                    Exchange Token
                </button>
            </div>
        {/if}

        {#if qrError}
            <p class="warning">{qrError}</p>
        {/if}

        {#if result}
            <pre class="result">{result}</pre>
        {/if}
    </section>
</main>

<style>
    .shell {
        padding: 2.5rem 1.2rem 4rem;
        max-width: 980px;
        margin: 0 auto;
    }

    header h1 {
        font-family: "Fraunces", serif;
        font-weight: 600;
        margin: 0.3rem 0;
        font-size: clamp(2rem, 5vw, 4rem);
        line-height: 1;
    }

    .shell {
        font-family: "Space Grotesk", sans-serif;
    }

    .eyebrow {
        text-transform: uppercase;
        letter-spacing: 0.12em;
        font-weight: 700;
        opacity: 0.7;
        margin: 0;
    }

    .sub {
        max-width: 42ch;
        margin-top: 0;
    }

    .auth-panel {
        margin-top: 1.5rem;
        border: 1px solid var(--line);
        border-radius: 1rem;
        background: var(--card);
        padding: 1rem;
        backdrop-filter: blur(8px);
    }

    .auth-layout {
        display: grid;
        grid-template-columns: minmax(140px, 240px) 1fr;
        gap: 1rem;
        align-items: start;
    }

    .qr-image {
        width: 100%;
        border-radius: 0.6rem;
        border: 1px solid var(--line);
        background: white;
        aspect-ratio: 1;
        object-fit: cover;
    }

    .qr-placeholder {
        width: 100%;
        border-radius: 0.6rem;
        border: 1px dashed var(--line);
        background: color-mix(in oklab, var(--surface) 88%, white 12%);
        aspect-ratio: 1;
        display: grid;
        place-items: center;
        color: color-mix(in oklab, var(--ink) 70%, white 30%);
        font-size: 0.95rem;
        text-align: center;
        padding: 0.75rem;
    }

    .token-input {
        width: 100%;
        margin-top: 0.8rem;
        border: 1px solid var(--line);
        border-radius: 0.7rem;
        padding: 0.7rem;
        font: inherit;
        background: var(--surface);
        color: var(--ink);
    }

    .auth-url {
        margin: 0.6rem 0 0;
        word-break: break-all;
    }

    .auth-url a {
        color: var(--accent);
        text-decoration: underline;
    }

    .auth-actions {
        display: flex;
        gap: 0.6rem;
        margin-top: 0.7rem;
        flex-wrap: wrap;
    }

    .auth-actions button,
    .link-btn {
        border: 0;
        border-radius: 0.6rem;
        padding: 0.6rem 0.9rem;
        font: inherit;
        cursor: pointer;
        text-decoration: none;
        display: inline-flex;
        align-items: center;
        justify-content: center;
    }

    .ghost {
        background: var(--ghost);
        color: var(--ink);
    }

    .solid {
        background: linear-gradient(140deg, var(--accent), #ff7e58);
        color: white;
    }

    .state {
        margin-top: 0;
    }

    .result {
        margin-top: 1rem;
        border: 1px solid var(--line);
        border-radius: 0.7rem;
        padding: 0.8rem;
        background: var(--surface);
        overflow-x: auto;
    }

    hr {
        border: 0;
        border-top: 1px solid var(--line);
        margin: 1rem 0;
    }

    @media (max-width: 780px) {
        .auth-layout {
            grid-template-columns: 1fr;
        }

        .qr-image {
            max-width: 260px;
        }
    }
</style>
