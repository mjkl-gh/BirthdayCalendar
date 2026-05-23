<script>
  import { onMount, tick } from "svelte";
  import QRCode from "qrcode";

  let birthdays = [];
  let loading = true;
  let error = "";
  let open = false;
  let formError = "";
  let formWarning = "";
  let formSuccess = "";
  let submitting = false;
  let isDarkMode = false;
  let sunsetLabel = "";
  let themeMode = "auto";
  let shouldFloatFab = false;
  let calendarWarning = "";
  let authRequired = false;
  let authLoading = false;
  let authError = "";
  let authTokenInput = "";
  let qrDataUrl = "";
  let qrExpiresAt = 0;

  const deviceTimeZone =
    Intl.DateTimeFormat().resolvedOptions().timeZone || "UTC";
  const themeTimerMs = 60_000;
  const themeStorageKey = "birthday-calendar-theme-mode";

  let form = {
    firstName: "",
    lastName: "",
    email: "",
    birthday: "",
    notes: "",
  };

  const monthNames = [
    "January",
    "February",
    "March",
    "April",
    "May",
    "June",
    "July",
    "August",
    "September",
    "October",
    "November",
    "December",
  ];

  function toNumberFromParts(parts, type) {
    return Number.parseInt(
      parts.find((part) => part.type === type)?.value || "0",
      10,
    );
  }

  function getLocalDateParts(date, timeZone) {
    const parts = new Intl.DateTimeFormat("en-CA", {
      timeZone,
      year: "numeric",
      month: "2-digit",
      day: "2-digit",
      hour: "2-digit",
      minute: "2-digit",
      second: "2-digit",
      hourCycle: "h23",
    }).formatToParts(date);

    return {
      year: toNumberFromParts(parts, "year"),
      month: toNumberFromParts(parts, "month"),
      day: toNumberFromParts(parts, "day"),
      hour: toNumberFromParts(parts, "hour"),
      minute: toNumberFromParts(parts, "minute"),
      second: toNumberFromParts(parts, "second"),
    };
  }

  function getDayOfYear(date, timeZone) {
    const { year, month, day } = getLocalDateParts(date, timeZone);
    const current = Date.UTC(year, month - 1, day);
    const start = Date.UTC(year, 0, 1);
    return Math.floor((current - start) / 86_400_000) + 1;
  }

  function getOffsetHours(date, timeZone) {
    const zoneText = new Intl.DateTimeFormat("en-US", {
      timeZone,
      timeZoneName: "shortOffset",
    })
      .formatToParts(date)
      .find((part) => part.type === "timeZoneName")?.value;

    const match = zoneText?.match(/^GMT([+-])(\d{1,2})(?::?(\d{2}))?$/);
    if (!match) {
      return 0;
    }

    const sign = match[1] === "-" ? -1 : 1;
    const hours = Number.parseInt(match[2], 10);
    const minutes = Number.parseInt(match[3] || "0", 10);
    return sign * (hours + minutes / 60);
  }

  // Timezone provides location context; latitude is estimated by region for
  // a practical sunset approximation without requiring geolocation permissions.
  function estimateLatitudeFromTimezone(timeZone) {
    if (
      timeZone.startsWith("Australia/") ||
      timeZone.startsWith("Pacific/Auckland") ||
      timeZone.startsWith("Pacific/Chatham") ||
      timeZone.startsWith("America/Argentina") ||
      timeZone.startsWith("America/Santiago") ||
      timeZone.startsWith("America/Sao_Paulo") ||
      timeZone.startsWith("America/Montevideo")
    ) {
      return -34;
    }
    if (timeZone.startsWith("Europe/")) return 48;
    if (timeZone.startsWith("America/")) return 39;
    if (timeZone.startsWith("Africa/")) return 15;
    if (timeZone.startsWith("Asia/")) return 31;
    return 35;
  }

  function estimateSunTimes(date, timeZone) {
    const latitudeDeg = estimateLatitudeFromTimezone(timeZone);
    const dayOfYear = getDayOfYear(date, timeZone);

    const gamma = ((2 * Math.PI) / 365) * (dayOfYear - 1);
    const eqTime =
      229.18 *
      (0.000075 +
        0.001868 * Math.cos(gamma) -
        0.032077 * Math.sin(gamma) -
        0.014615 * Math.cos(2 * gamma) -
        0.040849 * Math.sin(2 * gamma));
    const decl =
      0.006918 -
      0.399912 * Math.cos(gamma) +
      0.070257 * Math.sin(gamma) -
      0.006758 * Math.cos(2 * gamma) +
      0.000907 * Math.sin(2 * gamma) -
      0.002697 * Math.cos(3 * gamma) +
      0.00148 * Math.sin(3 * gamma);

    const latitude = (latitudeDeg * Math.PI) / 180;
    const zenith = (90.833 * Math.PI) / 180;
    const cosHourAngle =
      (Math.cos(zenith) - Math.sin(latitude) * Math.sin(decl)) /
      (Math.cos(latitude) * Math.cos(decl));
    const clampedCosHourAngle = Math.max(-1, Math.min(1, cosHourAngle));
    const hourAngleDeg = (Math.acos(clampedCosHourAngle) * 180) / Math.PI;

    const offsetHours = getOffsetHours(date, timeZone);
    const centralMeridian = offsetHours * 15;
    const estimatedLongitude = centralMeridian;
    const solarNoonMinutes =
      720 - eqTime + 4 * (centralMeridian - estimatedLongitude);
    const sunriseMinutes = solarNoonMinutes - 4 * hourAngleDeg;
    const sunsetMinutes = solarNoonMinutes + 4 * hourAngleDeg;
    return {
      sunriseMinutes,
      sunsetMinutes,
    };
  }

  function getLocalMinutes(date, timeZone) {
    const { hour, minute, second } = getLocalDateParts(date, timeZone);
    return hour * 60 + minute + second / 60;
  }

  function formatMinutes(value) {
    const normalized = ((Math.round(value) % 1440) + 1440) % 1440;
    const hours24 = Math.floor(normalized / 60);
    const minutes = normalized % 60;
    const suffix = hours24 >= 12 ? "PM" : "AM";
    const hours12 = hours24 % 12 === 0 ? 12 : hours24 % 12;
    return `${hours12}:${String(minutes).padStart(2, "0")} ${suffix}`;
  }

  function syncThemeToSunset() {
    const now = new Date();
    const { sunriseMinutes, sunsetMinutes } = estimateSunTimes(
      now,
      deviceTimeZone,
    );
    const currentMinutes = getLocalMinutes(now, deviceTimeZone);

    const autoDarkMode =
      currentMinutes >= ((sunsetMinutes % 1440) + 1440) % 1440 ||
      currentMinutes < ((sunriseMinutes % 1440) + 1440) % 1440;

    if (themeMode === "dark") {
      isDarkMode = true;
    } else if (themeMode === "light") {
      isDarkMode = false;
    } else {
      isDarkMode = autoDarkMode;
    }

    sunsetLabel = formatMinutes(sunsetMinutes);

    document.documentElement.classList.toggle("theme-dark", isDarkMode);
  }

  function setThemeMode(mode) {
    if (!["auto", "light", "dark"].includes(mode)) {
      return;
    }

    themeMode = mode;
    localStorage.setItem(themeStorageKey, mode);
    syncThemeToSunset();
  }

  function toggleTheme() {
    if (themeMode === "auto") {
      setThemeMode(isDarkMode ? "light" : "dark");
      return;
    }
    setThemeMode(themeMode === "dark" ? "light" : "dark");
  }

  function updateFabMode() {
    // If content extends past the viewport, keep FAB floating and always visible.
    shouldFloatFab =
      document.documentElement.scrollHeight > window.innerHeight + 4;
  }

  async function apiFetch(url, options = {}) {
    const response = await fetch(url, {
      credentials: "include",
      ...options,
      headers: {
        ...(options.headers || {}),
      },
    });

    if (response.status === 401) {
      authRequired = true;
      await loadQrToken();
    }
    return response;
  }

  async function loadQrToken() {
    authLoading = true;
    authError = "";
    try {
      const response = await fetch("/api/auth/qr", {
        credentials: "include",
      });
      if (!response.ok) {
        const payload = await response.json().catch(() => ({}));
        throw new Error(payload.error || "Could not load local QR token");
      }

      const payload = await response.json();
      authTokenInput = payload.token || "";
      qrExpiresAt = payload.expiresAt || 0;
      qrDataUrl = await QRCode.toDataURL(authTokenInput, {
        width: 240,
        margin: 1,
      });
    } catch (err) {
      authError = err.message;
      qrDataUrl = "";
    } finally {
      authLoading = false;
    }
  }

  async function exchangeAuthToken() {
    authError = "";

    try {
      const response = await fetch("/api/auth/exchange", {
        method: "POST",
        credentials: "include",
        headers: {
          "Content-Type": "application/json",
        },
        body: JSON.stringify({ token: authTokenInput }),
      });

      const payload = await response.json().catch(() => ({}));
      if (!response.ok) {
        throw new Error(payload.error || "Authentication failed");
      }

      authRequired = false;
      await loadBirthdays();
    } catch (err) {
      authError = err.message;
    }
  }

  function formatEpoch(epoch) {
    if (!epoch) {
      return "";
    }
    return new Date(epoch * 1000).toLocaleTimeString([], {
      hour: "2-digit",
      minute: "2-digit",
    });
  }

  $: groupedBirthdays = birthdays.reduce((groups, item) => {
    const month = Number.parseInt(item.monthDay?.slice(0, 2) || "0", 10);
    const monthLabel =
      month >= 1 && month <= 12 ? monthNames[month - 1] : "Unknown";
    const existingGroup = groups.find((group) => group.month === monthLabel);

    if (existingGroup) {
      existingGroup.items.push(item);
      return groups;
    }

    groups.push({ month: monthLabel, items: [item] });
    return groups;
  }, []);

  async function loadBirthdays() {
    loading = true;
    error = "";
    calendarWarning = "";

    try {
      const response = await apiFetch("/api/birthdays");
      calendarWarning = response.headers.get("X-Calendar-Warning") || "";
      if (!response.ok) {
        if (response.status === 401) {
          birthdays = [];
          return;
        }
        const payload = await response.json();
        throw new Error(payload.error || "Unable to load birthdays");
      }
      birthdays = await response.json();
      birthdays = birthdays.sort((a, b) =>
        a.monthDay.localeCompare(b.monthDay),
      );
    } catch (err) {
      error = err.message;
    } finally {
      loading = false;
      await tick();
      updateFabMode();
    }
  }

  async function submit() {
    formError = "";
    formWarning = "";
    formSuccess = "";

    if (!form.firstName || !form.lastName || !form.email || !form.birthday) {
      formError = "Please complete all required fields.";
      return;
    }

    submitting = true;
    try {
      const response = await apiFetch("/api/vcards", {
        method: "POST",
        headers: {
          "Content-Type": "application/json",
        },
        body: JSON.stringify(form),
      });

      if (response.status === 401) {
        throw new Error("Please authenticate using the local QR token first");
      }

      const payload = await response.json();
      if (!response.ok) {
        if (
          Array.isArray(payload.notifierErrors) &&
          payload.notifierErrors.length > 0
        ) {
          console.error("[Notifier] delivery failed:", payload.notifierErrors);
        }
        throw new Error(payload.error || "Submission failed");
      }

      if (payload.warning) {
        formWarning = payload.warning;
      }
      if (
        Array.isArray(payload.notifierErrors) &&
        payload.notifierErrors.length > 0
      ) {
        console.warn(
          "[Notifier] partial delivery failure:",
          payload.notifierErrors,
        );
      }

      formSuccess = "Submitted. Thank you!";
      form = {
        firstName: "",
        lastName: "",
        email: "",
        birthday: "",
        notes: "",
      };
      await loadBirthdays();
    } catch (err) {
      formError = err.message;
    } finally {
      submitting = false;
    }
  }

  function closeOnBackdropClick(event) {
    if (event.target === event.currentTarget) {
      open = false;
    }
  }

  $: if (!loading) {
    tick().then(updateFabMode);
  }

  onMount(() => {
    const storedMode = localStorage.getItem(themeStorageKey);
    if (["auto", "light", "dark"].includes(storedMode)) {
      themeMode = storedMode;
    }

    loadBirthdays();
    syncThemeToSunset();
    updateFabMode();
    const timer = setInterval(syncThemeToSunset, themeTimerMs);
    window.addEventListener("resize", updateFabMode);
    return () => {
      clearInterval(timer);
      window.removeEventListener("resize", updateFabMode);
    };
  });
</script>

<main class="shell">
  <button
    class="theme-toggle"
    on:click={toggleTheme}
    aria-label={isDarkMode ? "Switch to light mode" : "Switch to dark mode"}
    title={isDarkMode ? "Switch to light mode" : "Switch to dark mode"}
  >
    {#if isDarkMode}
      ☀
    {:else}
      ☾
    {/if}
  </button>

  <header>
    <p class="eyebrow">Shared iCal</p>
    <h1>Birthday Constellation</h1>
    <p class="sub">If someone is missing, tap + and send a vCard request.</p>
  </header>

  {#if authRequired}
    <section class="auth-panel">
      <h2>Authenticate To Continue</h2>
      <p>
        Open this page from your local network to retrieve the rotating token
        QR, then paste or scan the token below.
      </p>

      {#if authLoading}
        <p class="state">Loading local QR token...</p>
      {:else}
        {#if qrDataUrl}
          <img class="qr-image" src={qrDataUrl} alt="Auth token QR code" />
          <p class="state">Token expires at {formatEpoch(qrExpiresAt)}</p>
        {/if}

        <textarea
          class="token-input"
          rows="3"
          placeholder="Paste token"
          bind:value={authTokenInput}
        ></textarea>

        <div class="auth-actions">
          <button class="ghost" on:click={loadQrToken}>Refresh QR</button>
          <button class="solid" on:click={exchangeAuthToken}
            >Authenticate</button
          >
        </div>

        {#if authError}
          <p class="error">{authError}</p>
        {/if}
      {/if}
    </section>
  {/if}

  {#if !authRequired && loading}
    <p class="state">Loading birthdays...</p>
  {:else if !authRequired && calendarWarning}
    <p class="state warning">
      {calendarWarning}. Showing pending requests only.
    </p>
  {:else if !authRequired && error}
    <p class="state error">{error}</p>
  {:else if !authRequired && birthdays.length === 0}
    <p class="state">No birthdays found yet.</p>
  {:else if !authRequired}
    <section class="month-list">
      {#each groupedBirthdays as group, groupIndex}
        <div
          class="month-group"
          style={`animation-delay: ${groupIndex * 60}ms`}
        >
          <h2 class="month-title">{group.month}</h2>
          {#each group.items as item}
            <article class="card">
              <p class="date">{item.monthDay}</p>
              <h3>{item.name}</h3>
              <p>{item.date}</p>
            </article>
          {/each}
        </div>
      {/each}
    </section>
  {/if}

  {#if !authRequired}
    <button
      class="fab"
      class:floating={shouldFloatFab}
      on:click={() => (open = true)}
      aria-label="Add birthday"
    >
      +
    </button>
  {/if}

  {#if open}
    <div class="overlay" on:click={closeOnBackdropClick} role="presentation">
      <section class="modal">
        <h3>Create vCard Request</h3>
        <div class="fields">
          <input placeholder="First name" bind:value={form.firstName} />
          <input placeholder="Last name" bind:value={form.lastName} />
          <input type="email" placeholder="Email" bind:value={form.email} />
          <input type="date" bind:value={form.birthday} />
          <textarea
            placeholder="Notes (optional)"
            rows="3"
            bind:value={form.notes}
          ></textarea>
        </div>

        {#if formError}<p class="error">{formError}</p>{/if}
        {#if formWarning}<p class="warning">{formWarning}</p>{/if}
        {#if formSuccess}<p class="success">{formSuccess}</p>{/if}

        <div class="actions">
          <button class="ghost" on:click={() => (open = false)}>Close</button>
          <button class="solid" on:click={submit} disabled={submitting}>
            {submitting ? "Sending..." : "Send vCard"}
          </button>
        </div>
      </section>
    </div>
  {/if}
</main>

<style>
  .shell {
    padding: 2.5rem 1.2rem 5rem;
    max-width: 980px;
    margin: 0 auto;
  }

  :global(:root) {
    --content-rail-right: max(
      1.2rem,
      calc((100vw - min(980px, 100vw)) / 2 + 1.2rem)
    );
  }

  header h1 {
    font-family: "Fraunces", serif;
    margin: 0.3rem 0;
    font-size: clamp(2rem, 5vw, 4rem);
    line-height: 1;
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

  .theme-toggle {
    position: fixed;
    top: 1rem;
    right: var(--content-rail-right);
    z-index: 30;
    width: 2.6rem;
    height: 2.6rem;
    border: 1px solid var(--line);
    border-radius: 999px;
    background: var(--card);
    color: var(--ink);
    display: inline-grid;
    place-items: center;
    font-size: 1.2rem;
    line-height: 1;
    cursor: pointer;
    backdrop-filter: blur(8px);
  }

  .theme-toggle:hover {
    filter: brightness(1.05);
  }

  .theme-toggle:focus-visible {
    outline: 2px solid var(--accent);
    outline-offset: 2px;
  }

  .month-list {
    display: flex;
    flex-direction: column;
    gap: 1.4rem;
    margin-top: 2rem;
  }

  .month-group {
    display: flex;
    flex-direction: column;
    gap: 0.7rem;
    animation: rise 350ms ease forwards;
    opacity: 0;
  }

  .month-title {
    margin: 0;
    font-family: "Fraunces", serif;
    font-size: 1.35rem;
  }

  .auth-panel {
    margin-top: 1.5rem;
    border: 1px solid var(--line);
    border-radius: 1rem;
    background: var(--card);
    padding: 1rem;
  }

  .auth-panel h2 {
    margin: 0 0 0.5rem;
  }

  .qr-image {
    width: min(240px, 70vw);
    border-radius: 0.6rem;
    border: 1px solid var(--line);
    background: white;
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

  .auth-actions {
    display: flex;
    gap: 0.6rem;
    margin-top: 0.7rem;
  }

  .auth-actions button {
    border: 0;
    border-radius: 0.6rem;
    padding: 0.6rem 0.9rem;
    font: inherit;
    cursor: pointer;
  }

  .card {
    backdrop-filter: blur(8px);
    background: var(--card);
    border: 1px solid var(--line);
    border-radius: 1rem;
    padding: 1rem;
    width: 100%;
  }

  .card h3 {
    margin: 0.2rem 0;
    font-size: 1.05rem;
  }

  .date {
    margin: 0;
    color: var(--accent);
    font-weight: 700;
  }

  .state {
    margin-top: 1.5rem;
  }

  .error {
    color: #b52828;
  }

  .success {
    color: #1c7c35;
  }

  .warning {
    color: #9a5b00;
  }

  .fab {
    position: relative;
    display: grid;
    place-items: center;
    margin-top: 1rem;
    margin-left: auto;
    width: 3.6rem;
    height: 3.6rem;
    border: 0;
    border-radius: 999px;
    font-size: 2rem;
    line-height: 1;
    color: white;
    background: linear-gradient(140deg, var(--accent), #ff7e58);
    box-shadow: 0 12px 30px rgba(230, 95, 57, 0.35);
    cursor: pointer;
  }

  .fab.floating {
    position: fixed;
    right: var(--content-rail-right);
    bottom: 1.2rem;
    z-index: 30;
    margin-top: 0;
    margin-left: 0;
  }

  .overlay {
    position: fixed;
    inset: 0;
    background: rgba(17, 20, 31, 0.45);
    display: grid;
    place-items: center;
    padding: 1rem;
  }

  .modal {
    width: min(96vw, 550px);
    background: var(--surface);
    border: 1px solid var(--line);
    border-radius: 1rem;
    padding: 1rem;
  }

  .fields {
    display: grid;
    gap: 0.7rem;
    margin-top: 1rem;
  }

  input,
  textarea {
    border: 1px solid var(--line);
    border-radius: 0.7rem;
    padding: 0.7rem;
    font: inherit;
  }

  .actions {
    display: flex;
    justify-content: flex-end;
    gap: 0.6rem;
    margin-top: 1rem;
  }

  .actions button {
    border: 0;
    border-radius: 0.6rem;
    padding: 0.65rem 1rem;
    font: inherit;
    cursor: pointer;
  }

  .ghost {
    background: var(--ghost);
  }

  .solid {
    color: white;
    background: var(--ink);
  }

  @media (max-width: 640px) {
    .shell {
      padding-top: 1.8rem;
    }

    .theme-toggle {
      top: 0.7rem;
      right: var(--content-rail-right);
    }

    .fab.floating {
      right: var(--content-rail-right);
    }
  }
</style>
