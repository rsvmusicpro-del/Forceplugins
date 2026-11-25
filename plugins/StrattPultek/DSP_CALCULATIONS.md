# StrattPultek — 50 calculs mathématiques et physiques

Référentiel de conception et de vérification. Les équations servent à dimensionner ou contrôler la chaîne ; elles ne signifient pas que chaque opération est exécutée à chaque échantillon. Les calculs coûteux restent au taux de contrôle ou à prepare(), conformément au budget ARM MPC.

## Niveau, énergie et dynamique

1. Gain d'amplitude depuis les décibels : \(G=10^{L_{dB}/20}\).
2. Niveau depuis le gain : \(L_{dB}=20\log_{10}|G|\).
3. Rapport de puissance : \(P_2/P_1=10^{\Delta L_{dB}/10}\).
4. Rapport de tension à impédance égale : \(V_2/V_1=10^{\Delta L_{dB}/20}\).
5. Valeur efficace : \(x_{RMS}=\sqrt{N^{-1}\sum_{n=0}^{N-1}x_n^2}\).
6. Facteur de crête : \(C=|x|_{peak}/x_{RMS}\).
7. Marge avant écrêtage numérique : \(H=-20\log_{10}|x|_{peak}\) dBFS.
8. Énergie discrète : \(E=\sum_n x_n^2\).
9. Puissance moyenne : \(P=E/N\).
10. Corrélation stéréo normalisée : \(\rho=\frac{\sum LR}{\sqrt{\sum L^2\sum R^2}}\).

## Échantillonnage et filtres

11. Fréquence de Nyquist : \(f_N=f_s/2\).
12. Pulsation normalisée : \(\omega=2\pi f/f_s\).
13. Période d'échantillonnage : \(T_s=1/f_s\).
14. Coefficient d'un passe-bas 1er ordre : \(a=1-e^{-2\pi f_c/f_s}\).
15. Pôle discret associé : \(p=1-a=e^{-2\pi f_c/f_s}\).
16. Réponse du passe-bas : \(H(z)=a/(1-pz^{-1})\).
17. Réponse complexe à \(\omega\) : \(H(e^{j\omega})=a/(1-pe^{-j\omega})\).
18. Module du passe-bas : \(|H|=a/\sqrt{1+p^2-2p\cos\omega}\).
19. Retard de groupe : \(\tau_g(\omega)=-d\arg H(e^{j\omega})/d\omega\) échantillons.
20. Cascade de filtres : \(H_T(z)=\prod_i H_i(z)\).
21. Gains en cascade : \(L_T(f)=\sum_i20\log_{10}|H_i(f)|\).
22. Décalage d'octave : \(f_2/f_1=2\).
23. Décalage d'une demi-octave : \(f_2/f_1=\sqrt2\).
24. Fréquence géométrique : \(f_g=\sqrt{f_1f_2}\).
25. Coefficient biquad RBJ : \(\alpha=\sin(\omega_0)/(2Q)\).
26. Relation bande passante/Q : \(Q=f_0/(f_2-f_1)\) (approximation bande étroite).
27. Largeur en octaves : \(BW=\log_2(f_2/f_1)\).
28. Gain d'un filtre en décibels : \(A=10^{G_{dB}/40}\) pour les formules RBJ.
29. Réponse d'un biquad : \(H(z)=\frac{b_0+b_1z^{-1}+b_2z^{-2}}{1+a_1z^{-1}+a_2z^{-2}}\).
30. Stabilité d'un filtre IIR : pôles \(|p_i|<1\).

## Commandes, transitions et mélange

31. Constante de temps : \(\tau=1/(2\pi f_c)\).
32. Coefficient de lissage par bloc : \(\alpha=1-e^{-N/(\tau f_s)}\).
33. Lissage exponentiel : \(y_k=y_{k-1}+\alpha(x_k-y_{k-1})\).
34. Interpolation logarithmique : \(f(t)=e^{(1-t)\ln f_0+t\ln f_1}\).
35. Conversion d'un fader dB en gain : \(G_F=10^{F_{dB}/20}\).
36. Gain total en série : \(G_T=G_{trim}G_F\).
37. Mélange linéaire : \(y=x_d+(x_w-x_d)m\).
38. Mélange parallèle cohérent à somme unitaire : \(y=(1-m)x+mw\).
39. Gain de compensation du drive : \(G_D=1+kD\).
40. Compensation de niveau pré/post saturation : \(x_s=F(G_Dx)/G_D\).

## Non-linéarité, aliasing et robustesse

41. Courbe polynomiale C1 par morceaux : \(F(x)=x\) si \(|x|\le1\), transition polynomiale pour \(1<|x|<3\), plateau borné au-delà.
42. Asymétrie harmonique : \(F_a(x)=F(x)+\eta F(x)^2\).
43. Dérivée de la primitive : \(dI_F(x)/dx=F(x)\).
44. ADAA ordre 1 : \(y[n]=\frac{I_F(x[n])-I_F(x[n-1])}{x[n]-x[n-1]}\).
45. Limite ADAA quand \(x[n]\to x[n-1]\) : \(y[n]\to F(x[n])\).
46. Seuil relatif de stabilité numérique : \(|\Delta x|<\epsilon\max(1,|x_n|,|x_{n-1}|)\).
47. Densité de signal bornée : \(d[n]=\operatorname{clamp}(2|x[n]+x[n-1]|,0,1)\).
48. Mélange de coloration : \(g[n]=Grain\cdot Drive\cdot DriveMix\cdot d[n]\).
49. Filtre bloqueur DC : \(y[n]=x[n]-x[n-1]+p y[n-1]\), avec \(p=e^{-2\pi f_{DC}/f_s}\).
50. Vérification de crête de sortie : \(P_{out}=\max_n|G_T y[n]|\), puis \(H_{out}=-20\log_{10}(P_{out})\).

## Calculs effectivement appliqués dans la version 1.4.0

- Seuil ADAA relatif au niveau interne (46), pour réduire l'annulation numérique dans la différence des primitives près des entrées voisines.
- Conversion dB/gain du fader (35), gain total de fin de chaîne (36), et lissage temporel (32–33).
- Conservation de la somme unitaire du Drive Mix et du Dry/Wet global (37–38) : éviter un gain artificiel lors du mélange de signaux corrélés.
- Vérification des réponses de filtres et des sélecteurs HF au taux d'échantillonnage réel (11–20), calculée en préparation ou au changement de bloc, pas par appel trigonométrique à chaque échantillon.

Les équations 1–50 sont un catalogue d'analyse et de validation ; les appliquer toutes comme des traitements supplémentaires dégraderait le son ou augmenterait inutilement la charge CPU.
