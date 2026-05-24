import { defineConfig } from 'vite';
import { svelte } from '@sveltejs/vite-plugin-svelte';
import { resolve } from 'path';

export default defineConfig({
    plugins: [svelte()],
    build: {
        outDir: 'auth-dist',
        rollupOptions: {
            input: {
                auth: resolve(__dirname, 'auth.html')
            }
        }
    },
    server: {
        host: true,
        proxy: {
            '/api': {
                target: 'http://localhost:9001',
                changeOrigin: true,
            },
        },
    },
});
