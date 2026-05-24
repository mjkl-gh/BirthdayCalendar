import { mount } from 'svelte';
import Auth from './Auth.svelte';
import './app.css';

const app = mount(Auth, {
    target: document.getElementById('auth-root'),
});

export default app;
