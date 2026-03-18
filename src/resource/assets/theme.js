(() => {
  function ensureParticlesRoot() {
    let el = document.getElementById('particles');
    if (el) return el;
    el = document.createElement('div');
    el.id = 'particles';
    el.className = 'particles';
    document.body.prepend(el);
    return el;
  }

  function randomChoice(arr) {
    return arr[Math.floor(Math.random() * arr.length)];
  }

  function createParticles(opts = {}) {
    const {
      count = 30,
      colors = [
        'rgba(106, 13, 173, 0.55)',
        'rgba(139, 92, 246, 0.45)',
        'rgba(167, 139, 250, 0.40)',
        'rgba(196, 181, 253, 0.30)'
      ],
      minSize = 6,
      maxSize = 24,
      minDuration = 12,
      maxDuration = 22
    } = opts;

    const root = ensureParticlesRoot();
    root.replaceChildren();

    const n = Math.max(0, Math.min(120, Number(count) || 0));
    for (let i = 0; i < n; i++) {
      const particle = document.createElement('div');
      particle.className = 'particle';
      const size = Math.random() * (maxSize - minSize) + minSize;
      particle.style.width = `${size}px`;
      particle.style.height = `${size}px`;
      particle.style.left = `${Math.random() * 100}%`;
      particle.style.top = `${Math.random() * 110}%`;
      particle.style.background = randomChoice(colors);
      particle.style.animationDelay = `${Math.random() * 15}s`;
      particle.style.animationDuration = `${Math.random() * (maxDuration - minDuration) + minDuration}s`;
      root.appendChild(particle);
    }
  }

  window.ThemeParticles = { createParticles };

  window.addEventListener('load', () => {
    if (!document.body.classList.contains('theme')) return;
    createParticles();
  });
})();

