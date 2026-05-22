<script>
  import { onMount } from 'svelte';

  let birthdays = [];
  let loading = true;
  let error = '';
  let open = false;
  let formError = '';
  let formSuccess = '';
  let submitting = false;

  let form = {
    firstName: '',
    lastName: '',
    email: '',
    birthday: '',
    notes: '',
  };

  async function loadBirthdays() {
    loading = true;
    error = '';

    try {
      const response = await fetch('/api/birthdays');
      if (!response.ok) {
        const payload = await response.json();
        throw new Error(payload.error || 'Unable to load birthdays');
      }
      birthdays = await response.json();
      birthdays = birthdays.sort((a, b) => a.monthDay.localeCompare(b.monthDay));
    } catch (err) {
      error = err.message;
    } finally {
      loading = false;
    }
  }

  async function submit() {
    formError = '';
    formSuccess = '';

    if (!form.firstName || !form.lastName || !form.email || !form.birthday) {
      formError = 'Please complete all required fields.';
      return;
    }

    submitting = true;
    try {
      const response = await fetch('/api/vcards', {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
        },
        body: JSON.stringify(form),
      });

      const payload = await response.json();
      if (!response.ok) {
        throw new Error(payload.error || 'Submission failed');
      }

      formSuccess = 'Submitted. Thank you!';
      form = {
        firstName: '',
        lastName: '',
        email: '',
        birthday: '',
        notes: '',
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

  onMount(loadBirthdays);
</script>

<main class="shell">
  <header>
    <p class="eyebrow">Shared iCal</p>
    <h1>Birthday Constellation</h1>
    <p class="sub">If someone is missing, tap + and send a vCard request.</p>
  </header>

  {#if loading}
    <p class="state">Loading birthdays...</p>
  {:else if error}
    <p class="state error">{error}</p>
  {:else if birthdays.length === 0}
    <p class="state">No birthdays found yet.</p>
  {:else}
    <section class="grid">
      {#each birthdays as item, index}
        <article class="card" style={`animation-delay: ${index * 60}ms`}>
          <p class="date">{item.monthDay}</p>
          <h2>{item.name}</h2>
          <p>{item.date}</p>
        </article>
      {/each}
    </section>
  {/if}

  <button class="fab" on:click={() => (open = true)} aria-label="Add birthday">
    +
  </button>

  {#if open}
    <div class="overlay" on:click={closeOnBackdropClick} role="presentation">
      <section class="modal">
        <h3>Create vCard Request</h3>
        <div class="fields">
          <input placeholder="First name" bind:value={form.firstName} />
          <input placeholder="Last name" bind:value={form.lastName} />
          <input type="email" placeholder="Email" bind:value={form.email} />
          <input type="date" bind:value={form.birthday} />
          <textarea placeholder="Notes (optional)" rows="3" bind:value={form.notes}></textarea>
        </div>

        {#if formError}<p class="error">{formError}</p>{/if}
        {#if formSuccess}<p class="success">{formSuccess}</p>{/if}

        <div class="actions">
          <button class="ghost" on:click={() => (open = false)}>Close</button>
          <button class="solid" on:click={submit} disabled={submitting}>
            {submitting ? 'Sending...' : 'Send vCard'}
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

  header h1 {
    font-family: 'Fraunces', serif;
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

  .grid {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(170px, 1fr));
    gap: 1rem;
    margin-top: 2rem;
  }

  .card {
    backdrop-filter: blur(8px);
    background: var(--card);
    border: 1px solid var(--line);
    border-radius: 1rem;
    padding: 1rem;
    animation: rise 350ms ease forwards;
    opacity: 0;
  }

  .card h2 {
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

  .fab {
    position: fixed;
    right: 1rem;
    bottom: 1rem;
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
    background: white;
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
    background: #ececf4;
  }

  .solid {
    color: white;
    background: var(--ink);
  }

  @media (max-width: 640px) {
    .shell {
      padding-top: 1.8rem;
    }
  }
</style>